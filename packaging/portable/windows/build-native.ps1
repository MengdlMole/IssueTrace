[CmdletBinding()]
param(
    [string]$QtRoot = $env:QTDIR,
    [string]$MinGwRoot = "",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-Native {
    if ($args.Count -eq 0) { throw "Invoke-Native requires a command." }
    $command = [string]$args[0]
    $arguments = @($args | Select-Object -Skip 1)
    & $command @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $command"
    }
}

function Find-Executable {
    param([string[]]$Names)
    foreach ($name in $Names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($null -ne $command) { return $command.Source }
    }
    return $null
}

if ($env:OS -ne "Windows_NT") {
    throw "build-native.ps1 must run on Windows. Use build-cross-macos.sh on macOS."
}

$nativeArchitecture = if ($env:PROCESSOR_ARCHITEW6432) {
    $env:PROCESSOR_ARCHITEW6432
} else {
    $env:PROCESSOR_ARCHITECTURE
}
if ($nativeArchitecture -notin @("AMD64", "x86_64")) {
    throw "Only native Windows x86_64 packages are currently supported. Detected: $nativeArchitecture"
}

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$cmakeLists = Join-Path $repoRoot "CMakeLists.txt"
if ([string]::IsNullOrWhiteSpace($Version)) {
    $match = [regex]::Match([IO.File]::ReadAllText($cmakeLists),
        'project\(IssueTrace VERSION ([^ ]+)')
    if (-not $match.Success) { throw "Cannot read the project version from CMakeLists.txt." }
    $Version = $match.Groups[1].Value
}

if ([string]::IsNullOrWhiteSpace($QtRoot)) {
    $qtPathsOnPath = Find-Executable @("qtpaths6.exe", "qtpaths.exe")
    if ($null -eq $qtPathsOnPath) {
        throw "Pass -QtRoot C:\Qt\6.11.1\mingw_64 or set QTDIR."
    }
    $QtRoot = (& $qtPathsOnPath --query QT_INSTALL_PREFIX).Trim()
}
$QtRoot = [IO.Path]::GetFullPath($QtRoot)
$qtPaths = @(
    (Join-Path $QtRoot "bin\qtpaths6.exe"),
    (Join-Path $QtRoot "bin\qtpaths.exe")
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
$winDeployQt = Join-Path $QtRoot "bin\windeployqt.exe"
if ($null -eq $qtPaths -or -not (Test-Path -LiteralPath $winDeployQt)) {
    throw "QtRoot does not contain qtpaths and windeployqt: $QtRoot"
}
$qtVersion = (& $qtPaths --query QT_VERSION).Trim()
if ($qtVersion -ne "6.11.1") {
    throw "The Windows dependency lock pins Qt 6.11.1; detected $qtVersion at $QtRoot."
}

if ([string]::IsNullOrWhiteSpace($MinGwRoot)) {
    $gppOnPath = Find-Executable @("g++.exe")
    if ($null -ne $gppOnPath) {
        $MinGwRoot = Split-Path (Split-Path $gppOnPath -Parent) -Parent
    } else {
        $qtInstallRoot = Split-Path (Split-Path $QtRoot -Parent) -Parent
        $candidates = @(Get-ChildItem (Join-Path $qtInstallRoot "Tools") -Directory `
            -Filter "mingw*_64" -ErrorAction SilentlyContinue)
        if ($candidates.Count -eq 1) {
            $MinGwRoot = $candidates[0].FullName
        } elseif ($candidates.Count -gt 1) {
            $names = ($candidates.FullName -join ", ")
            throw "Multiple MinGW toolchains were found. Pass the one matching Qt 6.11.1 with -MinGwRoot. Candidates: $names"
        }
    }
}
if ([string]::IsNullOrWhiteSpace($MinGwRoot)) {
    throw "Pass the Qt MinGW toolchain directory with -MinGwRoot C:\Qt\Tools\mingw*_64."
}
$MinGwRoot = [IO.Path]::GetFullPath($MinGwRoot)
$mingwBin = Join-Path $MinGwRoot "bin"
$gcc = Join-Path $mingwBin "gcc.exe"
$gpp = Join-Path $mingwBin "g++.exe"
if (-not (Test-Path -LiteralPath $gcc) -or -not (Test-Path -LiteralPath $gpp)) {
    throw "MinGwRoot does not contain bin\gcc.exe and bin\g++.exe: $MinGwRoot"
}
$env:PATH = "$mingwBin;$env:PATH"
$compilerTarget = (& $gpp -dumpmachine).Trim()
if ($compilerTarget -notmatch '^x86_64-.*mingw') {
    throw "The selected compiler is not Windows x86_64 MinGW: $compilerTarget"
}

$cmake = Find-Executable @("cmake.exe", "cmake")
$ninja = Find-Executable @("ninja.exe", "ninja")
$git = Find-Executable @("git.exe", "git")
if ($null -eq $cmake -or $null -eq $ninja -or $null -eq $git) {
    throw "CMake, Ninja and Git must be available on PATH."
}

$buildDir = Join-Path $repoRoot "build\windows-x86_64-native-release"
$stageName = "IssueTrace-$Version-windows-x86_64"
$stageDir = Join-Path $repoRoot "build\portable\$stageName"
$artifactDir = Join-Path $repoRoot "build\artifacts"
$artifact = Join-Path $artifactDir "$stageName-dev.zip"
$cachedXlsxwriter = Join-Path $repoRoot "build\core-debug\_deps\libxlsxwriter-src"
$xlsxwriterSource = Join-Path $buildDir "_deps\libxlsxwriter-src"

$configureArguments = @(
    "-S", $repoRoot,
    "-B", $buildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_PREFIX_PATH=$QtRoot",
    "-DCMAKE_C_COMPILER=$gcc",
    "-DCMAKE_CXX_COMPILER=$gpp",
    "-DCMAKE_MAKE_PROGRAM=$ninja",
    "-DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=TRUE",
    "-DISSUETRACE_BUNDLED_ZLIB=ON",
    "-DISSUETRACE_BUILD_DESKTOP=ON"
)
if (Test-Path -LiteralPath (Join-Path $cachedXlsxwriter "License.txt")) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_LIBXLSXWRITER=$cachedXlsxwriter"
    $xlsxwriterSource = $cachedXlsxwriter
}

Invoke-Native $cmake @configureArguments
Invoke-Native $cmake --build $buildDir --parallel
Invoke-Native $cmake -E env "PATH=$mingwBin;$env:PATH" ctest --test-dir $buildDir `
    --output-on-failure

if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
Invoke-Native $cmake --install $buildDir --prefix $stageDir
foreach ($developmentDirectory in @("include", "lib")) {
    $path = Join-Path $stageDir $developmentDirectory
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
}

$deployHelp = (& $winDeployQt --help 2>&1 | Out-String)
$deployArguments = @(
    "--release",
    "--qmldir", (Join-Path $repoRoot "qml"),
    "--dir", $stageDir,
    "--no-translations"
)
foreach ($optionalFlag in @(
    "--no-opengl-sw",
    "--no-system-d3d-compiler",
    "--no-system-dxc-compiler"
)) {
    if ($deployHelp.Contains($optionalFlag)) { $deployArguments += $optionalFlag }
}
$deployArguments += (Join-Path $stageDir "IssueTrace.exe")
Invoke-Native $winDeployQt @deployArguments

# CMake's native Qt install deployment may have run before the explicit,
# constrained windeployqt pass. Remove optional redistributables that are not
# part of IssueTrace's open-source runtime policy.
Get-ChildItem $stageDir -File -Recurse | Where-Object {
    $_.Name -ieq "opengl32sw.dll" -or $_.Name -like "d3dcompiler_*.dll"
} | Remove-Item -Force

foreach ($requiredPayload in @(
    "IssueTrace.exe",
    "IssueTraceUpdater.exe",
    "Qt6Core.dll",
    "plugins\platforms\qwindows.dll",
    "qml\QtQuick\qmldir"
)) {
    if (-not (Test-Path -LiteralPath (Join-Path $stageDir $requiredPayload))) {
        throw "windeployqt did not produce required payload: $requiredPayload"
    }
}
$forbiddenPayload = Get-ChildItem $stageDir -File -Recurse | Where-Object {
    $_.Name -ieq "opengl32sw.dll" -or $_.Name -like "d3dcompiler_*.dll" -or `
        $_.Name -like "Qt6WebEngine*"
}
if ($null -ne $forbiddenPayload) {
    throw "Unexpected excluded runtime payload: $($forbiddenPayload.FullName -join ', ')"
}

Copy-Item (Join-Path $PSScriptRoot "qt.conf") $stageDir -Force
Copy-Item (Join-Path $repoRoot "README.md") $stageDir -Force
Copy-Item (Join-Path $repoRoot "docs") $stageDir -Recurse -Force
Copy-Item (Join-Path $PSScriptRoot "README.md") `
    (Join-Path $stageDir "PORTABLE_README.md") -Force
Copy-Item (Join-Path $repoRoot "packaging\dependency-lock.windows-x86_64.json") `
    $stageDir -Force

$licensesDir = Join-Path $stageDir "licenses"
New-Item -ItemType Directory -Path $licensesDir -Force | Out-Null
Copy-Item (Join-Path $xlsxwriterSource "License.txt") `
    (Join-Path $licensesDir "libxlsxwriter-License.txt") -Force
$zlibLicense = Join-Path $buildDir "_deps\zlib-src\LICENSE"
if (Test-Path -LiteralPath $zlibLicense) {
    Copy-Item $zlibLicense (Join-Path $licensesDir "zlib-License.txt") -Force
}
$qtLicenseCandidates = @(
    (Join-Path $QtRoot "LICENSES"),
    (Join-Path (Split-Path (Split-Path $QtRoot -Parent) -Parent) "Licenses"),
    (Join-Path (Split-Path (Split-Path $QtRoot -Parent) -Parent) `
        "Docs\Qt-$qtVersion\licenses")
)
$qtLicenses = $qtLicenseCandidates | Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if ($null -eq $qtLicenses) {
    $qtDestination = Join-Path $licensesDir "Qt"
    New-Item -ItemType Directory -Path $qtDestination -Force | Out-Null
    # IssueTrace and Qt Community are both distributed under GPL-3.0 in this
    # package, so the repository's complete GPL text is a valid fallback.
    Copy-Item (Join-Path $repoRoot "LICENSE") `
        (Join-Path $qtDestination "GPL-3.0.txt") -Force
} else {
    Copy-Item $qtLicenses (Join-Path $licensesDir "Qt") -Recurse -Force
}
$mingwLicenses = Join-Path $MinGwRoot "licenses"
if (-not (Test-Path -LiteralPath $mingwLicenses)) {
    throw "MinGW license texts were not found: $mingwLicenses"
}
Copy-Item $mingwLicenses (Join-Path $licensesDir "MinGW") -Recurse -Force

New-Item -ItemType Directory -Path $artifactDir -Force | Out-Null
Invoke-Native $cmake "-DOUTPUT=$(Join-Path $stageDir "IssueTrace-$Version.cdx.json")" `
    "-DVERSION=$Version" "-DPLATFORM=windows" "-DARCHITECTURE=x86_64" `
    "-DQT_VERSION=$qtVersion" "-DSQLITE_VERSION=3.53.4" "-DZLIB_VERSION=1.3.2" `
    "-DTOOLCHAIN=MinGW-w64" -P (Join-Path $repoRoot "packaging\generate-sbom.cmake")
Invoke-Native $cmake "-DPACKAGE_ROOT=$stageDir" "-DVERSION=$Version" `
    "-DPLATFORM=windows" "-DARCHITECTURE=x86_64" `
    -P (Join-Path $repoRoot "packaging\generate-release-manifest.cmake")
if (Test-Path -LiteralPath $artifact) { Remove-Item -LiteralPath $artifact -Force }
Invoke-Native $cmake -E chdir (Split-Path $stageDir -Parent) $cmake -E tar cf `
    $artifact --format=zip -- $stageName
Invoke-Native $cmake "-DARTIFACT=$artifact" `
    -P (Join-Path $repoRoot "packaging\generate-archive-checksum.cmake")
Invoke-Native (Join-Path $repoRoot "tests\portable_package_smoke_windows.ps1") `
    -TestRoot (Join-Path $repoRoot "build\portable-smoke\windows-x86_64") `
    -Archive $artifact -PackageName $stageName -Version $Version

Write-Host "Windows portable package: $artifact"
Write-Host "SHA-256 file: ${artifact}.sha256"
