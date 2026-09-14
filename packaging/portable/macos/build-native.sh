#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$script_dir/../../.." && pwd)"
qt_prefix="${1:-/opt/homebrew}"
version="${2:-$(sed -n 's/^project(IssueTrace VERSION \([^ ]*\).*/\1/p' "$repo_dir/CMakeLists.txt")}"
machine="$(uname -m)"
case "$machine" in
  arm64|aarch64) architecture=arm64 ;;
  x86_64|amd64) architecture=x86_64 ;;
  *) printf 'Unsupported macOS architecture: %s\n' "$machine" >&2; exit 1 ;;
esac
build_dir="$repo_dir/build/macos-$architecture-release"
stage_name="IssueTrace-$version-macos-$architecture"
stage_dir="$repo_dir/build/portable/$stage_name"
artifact_dir="$repo_dir/build/artifacts"
artifact="$artifact_dir/$stage_name-dev.zip"
cached_xlsxwriter="$repo_dir/build/core-debug/_deps/libxlsxwriter-src"

test -f "$qt_prefix/lib/cmake/Qt6/Qt6Config.cmake"
qt_version="$("$qt_prefix/bin/qtpaths" --query QT_VERSION)"
configure_args=()
xlsxwriter_source="$build_dir/_deps/libxlsxwriter-src"
if test -f "$cached_xlsxwriter/License.txt"; then
  configure_args+=("-DFETCHCONTENT_SOURCE_DIR_LIBXLSXWRITER=$cached_xlsxwriter")
  xlsxwriter_source="$cached_xlsxwriter"
fi
cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DISSUETRACE_BUILD_DESKTOP=ON \
  -DCMAKE_PREFIX_PATH="$qt_prefix" "${configure_args[@]}"
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
cmake -E rm -rf "$stage_dir"
cmake --install "$build_dir" --prefix "$stage_dir"
cmake -E rm -rf "$stage_dir/include" "$stage_dir/lib"
# Qt's generic QML deployment discovers development-only test/tooling modules.
# They are not imported by IssueTrace and are removed before the package manifest.
cmake -E rm -rf \
  "$stage_dir/IssueTrace.app/Contents/Resources/qml/QtTest" \
  "$stage_dir/IssueTrace.app/Contents/Resources/qml/QtQuick/tooling" \
  "$stage_dir/IssueTrace.app/Contents/Frameworks/QtTest.framework" \
  "$stage_dir/IssueTrace.app/Contents/Frameworks/QtQuickTest.framework" \
  "$stage_dir/IssueTrace.app/Contents/PlugIns/libquicktestplugin.dylib" \
  "$stage_dir/IssueTrace.app/Contents/PlugIns/libquicktoolingplugin.dylib"
# Keep Qt's headless platform plugin in release archives so the exact portable
# payload can be startup-tested in CI without a logged-in desktop session.
cmake -E copy "$qt_prefix/share/qt/plugins/platforms/libqoffscreen.dylib" \
  "$stage_dir/IssueTrace.app/Contents/PlugIns/platforms/libqoffscreen.dylib"
cmake -E copy "$repo_dir/README.md" "$stage_dir/README.md"
cmake -E copy_directory "$repo_dir/docs" "$stage_dir/docs"
cmake -E copy "$script_dir/README.md" "$stage_dir/PORTABLE_README.md"
cmake -E copy "$repo_dir/packaging/dependency-lock.macos-arm64.json" \
  "$stage_dir/dependency-lock.macos-$architecture.json"
cmake -E make_directory "$stage_dir/licenses/Qt"
if test -d "$qt_prefix/share/qt/LICENSES"; then
  cmake -E copy_directory "$qt_prefix/share/qt/LICENSES" "$stage_dir/licenses/Qt"
fi
cmake -E copy "$xlsxwriter_source/License.txt" \
  "$stage_dir/licenses/libxlsxwriter-License.txt"
cmake -E make_directory "$artifact_dir"
cmake -DOUTPUT="$stage_dir/IssueTrace-$version.cdx.json" -DVERSION="$version" \
  -DPLATFORM=macos -DARCHITECTURE="$architecture" -DQT_VERSION="$qt_version" \
  -DSQLITE_VERSION=3.43.2-system -DZLIB_VERSION=1.2.12-system \
  -DTOOLCHAIN=LLVM -P "$repo_dir/packaging/generate-sbom.cmake"
cmake -DPACKAGE_ROOT="$stage_dir" -DVERSION="$version" -DPLATFORM=macos \
  -DARCHITECTURE="$architecture" \
  -P "$repo_dir/packaging/generate-release-manifest.cmake"
cmake -E chdir "$(dirname "$stage_dir")" cmake -E tar cf "$artifact" \
  --format=zip -- "$stage_name"
cmake -DARTIFACT="$artifact" \
  -P "$repo_dir/packaging/generate-archive-checksum.cmake"
cmake -DTEST_ROOT="$repo_dir/build/portable-smoke/macos-$architecture" \
  -DARCHIVE="$artifact" -DPACKAGE_NAME="$stage_name" -DVERSION="$version" \
  -P "$repo_dir/tests/portable_package_smoke.cmake"
