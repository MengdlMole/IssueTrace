#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"
generator="$repo_dir/build/icon-generator"
iconset="$repo_dir/build/IssueTrace.iconset"
svg="$repo_dir/resources/icons/issuetrace.svg"

command -v pkg-config >/dev/null
cmake -E make_directory "$repo_dir/build"
cmake -E rm -rf "$iconset"

c++ -std=c++20 "$script_dir/generate-icons.cpp" -o "$generator" \
  $(pkg-config --cflags --libs Qt6Svg)
QT_QPA_PLATFORM=offscreen "$generator" "$svg" "$iconset" \
  "$repo_dir/resources/icons/IssueTrace.ico" \
  "$repo_dir/resources/icons/IssueTrace.icns"
cmake -E copy "$iconset/icon_128x128@2x.png" \
  "$repo_dir/resources/icons/issuetrace-256.png"

printf 'Generated IssueTrace.icns, IssueTrace.ico and issuetrace-256.png\n'
