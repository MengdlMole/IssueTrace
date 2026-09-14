#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$script_dir/../../.." && pwd)"
qt_target="${1:-$repo_dir/build/toolchains/Qt}"
qt_host="${2:-/opt/homebrew}"
project_version="$(sed -n 's/^project(IssueTrace VERSION \([^ ]*\).*/\1/p' "$repo_dir/CMakeLists.txt")"
version="${3:-$project_version}"
build_dir="$repo_dir/build/windows-x86_64"
stage_name="IssueTrace-$version-windows-x86_64"
stage_dir="$repo_dir/build/portable/$stage_name"
artifact_dir="$repo_dir/build/artifacts"
artifact="$artifact_dir/$stage_name-dev.zip"
toolchain="$repo_dir/cmake/toolchains/windows-mingw-x86_64.cmake"
cached_xlsxwriter="$repo_dir/build/core-debug/_deps/libxlsxwriter-src"

test -n "$version"
test -f "$qt_target/lib/cmake/Qt6/qt.toolchain.cmake"
test -f "$qt_host/lib/cmake/Qt6/Qt6Config.cmake"
command -v x86_64-w64-mingw32-g++ >/dev/null
command -v x86_64-w64-mingw32-objdump >/dev/null

# A stale staged QML tree can be discovered by Qt's import scanner during the
# next configure. Remove it before configuration as well as before installation.
cmake -E rm -rf "$stage_dir"
configure_args=()
xlsxwriter_source="$build_dir/_deps/libxlsxwriter-src"
if test -f "$cached_xlsxwriter/License.txt"; then
  configure_args+=("-DFETCHCONTENT_SOURCE_DIR_LIBXLSXWRITER=$cached_xlsxwriter")
  xlsxwriter_source="$cached_xlsxwriter"
fi
cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DISSUETRACE_BUILD_DESKTOP=ON \
  -DCMAKE_TOOLCHAIN_FILE="$qt_target/lib/cmake/Qt6/qt.toolchain.cmake" \
  -DQT_CHAINLOAD_TOOLCHAIN_FILE="$toolchain" \
  -DQT_HOST_PATH="$qt_host" \
  -DQT_HOST_PATH_CMAKE_DIR="$qt_host/lib/cmake" \
  "${configure_args[@]}"
cmake --build "$build_dir" --parallel

cmake -E rm -rf "$stage_dir"
cmake --install "$build_dir" --prefix "$stage_dir"
# FetchContent dependencies install development headers/static libraries by
# default. They are already linked into IssueTrace.exe and do not belong in the
# end-user portable archive.
cmake -E rm -rf "$stage_dir/include" "$stage_dir/lib"
cmake -E copy "$script_dir/qt.conf" "$stage_dir/qt.conf"
cmake -E copy "$repo_dir/README.md" "$stage_dir/README.md"
cmake -E copy "$script_dir/README.md" "$stage_dir/PORTABLE_README.md"
cmake -E copy "$repo_dir/packaging/dependency-lock.windows-x86_64.json" \
  "$stage_dir/dependency-lock.windows-x86_64.json"
cmake -E make_directory "$stage_dir/licenses"
cmake -E copy "$xlsxwriter_source/License.txt" \
  "$stage_dir/licenses/libxlsxwriter-License.txt"

# Cross deployment cannot run the target qml tooling. Copy a conservative QML
# runtime set, then resolve its complete non-system DLL closure below. This is
# intentionally larger than the desired release bundle and must be minimized
# and smoke-tested on native Windows before a stable release.
cmake -E copy_directory "$qt_target/qml/QtQuick" "$stage_dir/qml/QtQuick"
cmake -E rm -rf "$stage_dir/qml/QtQuick/tooling"
cmake -E copy_directory "$qt_target/qml/QtQml" "$stage_dir/qml/QtQml"
cmake -E copy_directory "$qt_target/qml/Qt/labs/folderlistmodel" \
  "$stage_dir/qml/Qt/labs/folderlistmodel"
for plugin_group in platforms styles imageformats networkinformation tls; do
  if test -d "$qt_target/plugins/$plugin_group"; then
    cmake -E copy_directory "$qt_target/plugins/$plugin_group" \
      "$stage_dir/plugins/$plugin_group"
  fi
done

mingw_root="$(x86_64-w64-mingw32-gcc -print-sysroot)"

copy_dependency() {
  local dependency="$1"
  local lower
  local source
  lower="$(printf '%s' "$dependency" | tr '[:upper:]' '[:lower:]')"
  case "$lower" in
    api-ms-win-*|ext-ms-win-*|kernel32.dll|user32.dll|advapi32.dll|authz.dll|\
    avrt.dll|bcrypt.dll|cfgmgr32.dll|dnsapi.dll|gdi32.dll|\
    shell32.dll|shlwapi.dll|ole32.dll|oleaut32.dll|comdlg32.dll|comctl32.dll|\
    crypt32.dll|d2d1.dll|d3d9.dll|d3d11.dll|d3d12.dll|dwrite.dll|dxgi.dll|\
    dxguid.dll|dwmapi.dll|\
    imm32.dll|iphlpapi.dll|mpr.dll|msimg32.dll|msvcrt.dll|netapi32.dll|ncrypt.dll|\
    normaliz.dll|ntdll.dll|powrprof.dll|propsys.dll|psapi.dll|rpcrt4.dll|\
    secur32.dll|setupapi.dll|\
    shcore.dll|userenv.dll|uxtheme.dll|version.dll|winhttp.dll|winmm.dll|\
    wininet.dll|winspool.drv|wintrust.dll|wlanapi.dll|wldap32.dll|ws2_32.dll|wtsapi32.dll)
      return 0
      ;;
  esac
  if find "$stage_dir" -type f -iname "$dependency" -print -quit | grep -q .; then
    return 0
  fi
  source="$(find "$qt_target/bin" "$mingw_root" -type f -iname "$dependency" \
    -print -quit 2>/dev/null || true)"
  if test -z "$source"; then
    printf 'Unresolved Windows runtime dependency: %s\n' "$dependency" >&2
    return 1
  fi
  cmake -E copy "$source" "$stage_dir/$dependency"
}

# Resolve the complete non-system DLL closure from the application, plugins and
# QML modules. More files can be discovered after each pass, hence the loop.
for _pass in 1 2 3 4 5 6 7 8; do
  before="$(find "$stage_dir" -type f -iname '*.dll' | wc -l | tr -d ' ')"
  while IFS= read -r binary; do
    while IFS= read -r dependency; do
      test -n "$dependency" && copy_dependency "$dependency"
    done < <(x86_64-w64-mingw32-objdump -p "$binary" 2>/dev/null \
      | sed -n 's/.*DLL Name: //p')
  done < <(find "$stage_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \))
  after="$(find "$stage_dir" -type f -iname '*.dll' | wc -l | tr -d ' ')"
  test "$before" = "$after" && break
done

# Fail if any non-system dependency remains unresolved.
while IFS= read -r binary; do
  while IFS= read -r dependency; do
    test -n "$dependency" && copy_dependency "$dependency"
  done < <(x86_64-w64-mingw32-objdump -p "$binary" 2>/dev/null \
    | sed -n 's/.*DLL Name: //p')
done < <(find "$stage_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \))

cmake -DOUTPUT="$stage_dir/IssueTrace-$version.cdx.json" -DVERSION="$version" \
  -DPLATFORM=windows -DARCHITECTURE=x86_64 -DQT_VERSION=6.11.1 \
  -DTOOLCHAIN=MinGW-w64 \
  -P "$repo_dir/packaging/generate-sbom.cmake"

cmake \
  -DPACKAGE_ROOT="$stage_dir" \
  -DVERSION="$version" \
  -DPLATFORM=windows \
  -DARCHITECTURE=x86_64 \
  -P "$repo_dir/packaging/generate-release-manifest.cmake"

cmake -E make_directory "$artifact_dir"
cmake -E chdir "$(dirname "$stage_dir")" cmake -E tar cf "$artifact" \
  --format=zip -- "$stage_name"
cmake -DARTIFACT="$artifact" \
  -P "$repo_dir/packaging/generate-archive-checksum.cmake"
