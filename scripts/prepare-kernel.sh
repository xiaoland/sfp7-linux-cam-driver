#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Apply the exact tested camera composition to linux-surface's v6.19.8 base.
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: %s /path/to/clean/linux-surface-checkout\n' "$0" >&2
  exit 2
fi

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
tree=$(cd "$1" && pwd)
expected_commit=57d61aff0b53b089227f5a794363fec829114fc5

if [[ $(git -C "$tree" rev-parse HEAD) != "$expected_commit" ]]; then
  printf 'expected linux-surface v6.19.8-based commit %s\n' "$expected_commit" >&2
  exit 2
fi
if [[ -n $(git -C "$tree" status --porcelain) ]]; then
  printf 'kernel checkout must be clean before applying patches\n' >&2
  exit 2
fi

apply_one() {
  local patch=$1
  git -C "$tree" apply --index --check --binary "$patch"
  git -C "$tree" apply --index --binary "$patch"
}

check_tree() {
  local expected=$1
  local actual
  actual=$(git -C "$tree" write-tree)
  if [[ "$actual" != "$expected" ]]; then
    printf 'source tree mismatch: expected %s, got %s\n' "$expected" "$actual" >&2
    exit 1
  fi
}

while IFS= read -r -d '' patch; do
  apply_one "$patch"
done < <(find "$source_dir/kernel/ipu4-next-v6.19" -name '*.patch' -print0 | sort -z)
check_tree f8c29814a956624c891f1889dc0e760779fbe466

apply_one "$source_dir/kernel/thisiscamk/0001-thisiscamk-sp7-snapshot-overlay.patch"
check_tree 99184946d2625b8674faabab7a21d98525df5258

apply_one "$source_dir/kernel/thisiscamk-surface-ov5693-no-binning/0001-ov5693-disable-binned-modes.patch"
check_tree 7fe4572a7046b7915fad6927cb957cbf947e7f44

apply_one "$source_dir/kernel/runtime-final-net.patch"
check_tree 65c10d4c4f7b651204757b62d608755cc4c21779

printf 'prepared exact tested source tree %s\n' "$(git -C "$tree" write-tree)"
