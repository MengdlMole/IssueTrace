#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"
version="${1:-$(sed -n 's/^project(IssueTrace VERSION \([^ ]*\).*/\1/p' "$repo_dir/CMakeLists.txt")}"
artifact_dir="$repo_dir/build/artifacts"
stage_name="IssueTrace-$version-source"
stage_parent="$repo_dir/build/source-package"
stage_dir="$stage_parent/$stage_name"
artifact="$artifact_dir/$stage_name.tar.gz"

test -n "$version"
cmake -E rm -rf "$stage_dir"
cmake -E make_directory "$stage_dir"

for entry in CMakeLists.txt CMakePresets.json LICENSE README.md \
  THIRD_PARTY_NOTICES.md cmake docs include packaging qml resources src tests; do
  if test -d "$repo_dir/$entry"; then
    cmake -E copy_directory "$repo_dir/$entry" "$stage_dir/$entry"
  else
    cmake -E copy "$repo_dir/$entry" "$stage_dir/$entry"
  fi
done

# Source packages must not recursively contain previous artifacts or generated
# build trees. Only the packaging scripts and dependency evidence are included.
cmake -E rm -rf "$stage_dir/packaging/portable/.DS_Store"
cmake -E make_directory "$artifact_dir"
cmake -E chdir "$stage_parent" cmake -E tar czf "$artifact" -- "$stage_name"
cmake -DARTIFACT="$artifact" \
  -P "$repo_dir/packaging/generate-archive-checksum.cmake"
