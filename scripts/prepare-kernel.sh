#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Compatibility command name; output must now be a NEW directory.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  printf 'usage: %s /path/to/new/linux-sfp7-directory\n' "$0" >&2
  exit 2
fi
source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
exec python3 "$source_dir/scripts/source.py" prepare --component kernel --profile p1-runtime --output "$1"
