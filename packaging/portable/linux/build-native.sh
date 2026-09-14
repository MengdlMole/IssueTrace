#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$script_dir/../../.." && pwd)"
qt_prefix="${1:-/usr}"
version="${2:-$(sed -n 's/^project(IssueTrace VERSION \([^ ]*\).*/\1/p' "$repo_dir/CMakeLists.txt")}"
machine="$(uname -m)"
case "$machine" in
  x86_64|amd64) architecture=x86_64 ;;
  arm64|aarch64) architecture=arm64 ;;
  *) printf 'Unsupported Linux architecture: %s\n' "$machine" >&2; exit 1 ;;
esac
build_dir="$repo_dir/build/linux-$architecture-release"
stage_name="IssueTrace-$version-linux-$architecture"
stage_dir="$repo_dir/build/portable/$stage_name"
artifact_dir="$repo_dir/build/artifacts"
artifact="$artifact_dir/$stage_name.tar.gz"
cached_xlsxwriter="$repo_dir/build/core-debug/_deps/libxlsxwriter-src"

configure_args=()
xlsxwriter_source="$build_dir/_deps/libxlsxwriter-src"
if test -f "$cached_xlsxwriter/License.txt"; then
  configure_args+=("-DFETCHCONTENT_SOURCE_DIR_LIBXLSXWRITER=$cached_xlsxwriter")
  xlsxwriter_source="$cached_xlsxwriter"
fi
cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DISSUETRACE_BUILD_DESKTOP=ON \
  -DCMAKE_PREFIX_PATH="$qt_prefix" "${configure_args[@]}"
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
cmake -E rm -rf "$stage_dir"
cmake --install "$build_dir" --prefix "$stage_dir"
cmake -E rm -rf "$stage_dir/include" "$stage_dir/lib/cmake" \
  "$stage_dir/lib/pkgconfig"
for static_archive in "$stage_dir"/lib/*.a; do
  if test -f "$static_archive"; then
    cmake -E rm -f "$static_archive"
  fi
done
cmake -E copy "$repo_dir/README.md" "$stage_dir/README.md"
cmake -E copy "$script_dir/README.md" "$stage_dir/PORTABLE_README.md"
cmake -E copy "$repo_dir/packaging/dependency-lock.linux.json" \
  "$stage_dir/dependency-lock.linux-$architecture.json"
cmake -E copy "$script_dir/IssueTrace" "$stage_dir/IssueTrace"
chmod 755 "$stage_dir/IssueTrace"
if test -x "$qt_prefix/bin/qtpaths6"; then
  qtpaths_command="$qt_prefix/bin/qtpaths6"
elif test -x "$qt_prefix/bin/qtpaths"; then
  qtpaths_command="$qt_prefix/bin/qtpaths"
elif command -v qtpaths6 >/dev/null; then
  qtpaths_command=qtpaths6
else
  qtpaths_command=qtpaths
fi
qt_version="$("$qtpaths_command" --query QT_VERSION)"
sqlite_version="$(sqlite3 --version 2>/dev/null | awk '{print $1}' || printf 'system')"
zlib_version="$(pkg-config --modversion zlib 2>/dev/null || printf 'system')"
cmake -E make_directory "$stage_dir/licenses/Qt"
if test -d "$qt_prefix/share/qt6/LICENSES"; then
  cmake -E copy_directory "$qt_prefix/share/qt6/LICENSES" "$stage_dir/licenses/Qt"
elif test -d "$qt_prefix/share/qt/LICENSES"; then
  cmake -E copy_directory "$qt_prefix/share/qt/LICENSES" "$stage_dir/licenses/Qt"
fi
cmake -E copy "$xlsxwriter_source/License.txt" \
  "$stage_dir/licenses/libxlsxwriter-License.txt"
cmake -E make_directory "$artifact_dir"
cmake -DOUTPUT="$stage_dir/IssueTrace-$version.cdx.json" -DVERSION="$version" \
  -DPLATFORM=linux -DARCHITECTURE="$architecture" -DQT_VERSION="$qt_version" \
  -DSQLITE_VERSION="$sqlite_version-system" -DZLIB_VERSION="$zlib_version-system" \
  -DTOOLCHAIN=GCC -P "$repo_dir/packaging/generate-sbom.cmake"
cmake -DPACKAGE_ROOT="$stage_dir" -DVERSION="$version" -DPLATFORM=linux \
  -DARCHITECTURE="$architecture" \
  -P "$repo_dir/packaging/generate-release-manifest.cmake"
cmake -E chdir "$(dirname "$stage_dir")" cmake -E tar czf "$artifact" \
  -- "$stage_name"
cmake -DARTIFACT="$artifact" \
  -P "$repo_dir/packaging/generate-archive-checksum.cmake"
