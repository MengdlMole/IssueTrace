[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$TestRoot,
    [Parameter(Mandatory = $true)][string]$Archive,
    [Parameter(Mandatory = $true)][string]$PackageName,
    [Parameter(Mandatory = $true)][string]$Version
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-Checked {
    if ($args.Count -eq 0) { throw "Invoke-Checked requires a command." }
    $command = [string]$args[0]
    $arguments = @($args | Select-Object -Skip 1)
    & $command @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $command"
    }
}

$TestRoot = [IO.Path]::GetFullPath($TestRoot)
$Archive = [IO.Path]::GetFullPath($Archive)
if (-not (Test-Path -LiteralPath $Archive -PathType Leaf)) {
    throw "Portable archive does not exist: $Archive"
}
if (Test-Path -LiteralPath $TestRoot) {
    Remove-Item -LiteralPath $TestRoot -Recurse -Force
}
$currentRoot = Join-Path $TestRoot "current"
New-Item -ItemType Directory -Path $currentRoot -Force | Out-Null

try {
    Push-Location $currentRoot
    try { Invoke-Checked cmake.exe -E tar xf $Archive } finally { Pop-Location }
    $installRoot = Join-Path $currentRoot $PackageName
    if (-not (Test-Path -LiteralPath $installRoot)) {
        throw "Archive does not contain the expected root directory: $PackageName"
    }

    $manifest = Join-Path $installRoot "release-manifest.json"
    $content = [IO.File]::ReadAllText($manifest).Replace(
        '"version": "' + $Version + '"', '"version": "0.1.0"')
    [IO.File]::WriteAllText($manifest, $content, [Text.UTF8Encoding]::new($false))

    $updater = Join-Path $TestRoot "IssueTraceUpdater.exe"
    Copy-Item (Join-Path $installRoot "IssueTraceUpdater.exe") $updater -Force
    $oldExitAfterHealth = $env:ISSUETRACE_EXIT_AFTER_HEALTH
    $oldWorkspace = $env:ISSUETRACE_WORKSPACE
    $oldQpaPlatform = $env:QT_QPA_PLATFORM
    try {
        $env:ISSUETRACE_EXIT_AFTER_HEALTH = "1"
        $env:ISSUETRACE_WORKSPACE = Join-Path $TestRoot "workspace"
        if (Test-Path (Join-Path $installRoot "plugins\platforms\qoffscreen.dll")) {
            $env:QT_QPA_PLATFORM = "offscreen"
        }
        Invoke-Checked $updater --archive $Archive --install-root $installRoot `
            --executable "IssueTrace.exe" --wait-pid "4294967294"

        $updatedManifest = [IO.File]::ReadAllText(
            (Join-Path $installRoot "release-manifest.json"))
        $previousManifest = [IO.File]::ReadAllText(
            (Join-Path "$installRoot.previous" "release-manifest.json"))
        if (-not $updatedManifest.Contains('"version": "' + $Version + '"')) {
            throw "Portable package did not install version $Version."
        }
        if (-not $previousManifest.Contains('"version": "0.1.0"')) {
            throw "Portable package did not retain the previous version."
        }

        $env:ISSUETRACE_WORKSPACE = Join-Path $TestRoot "scroll-workspace"
        Invoke-Checked (Join-Path $installRoot "IssueTrace.exe") --verify-scroll-layout
        $env:ISSUETRACE_WORKSPACE = Join-Path $TestRoot "status-workspace"
        Invoke-Checked (Join-Path $installRoot "IssueTrace.exe") --verify-status-refresh
        $env:ISSUETRACE_WORKSPACE = Join-Path $TestRoot "custom-reminder-workspace"
        Invoke-Checked (Join-Path $installRoot "IssueTrace.exe") --verify-custom-reminder
    } finally {
        $env:ISSUETRACE_EXIT_AFTER_HEALTH = $oldExitAfterHealth
        $env:ISSUETRACE_WORKSPACE = $oldWorkspace
        $env:QT_QPA_PLATFORM = $oldQpaPlatform
    }
} finally {
    if (Test-Path -LiteralPath $TestRoot) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force
    }
}
