#!/usr/bin/env bash
set -euo pipefail

# Sync submodules to pinned commits recorded in doc/submodule_versions.md.
# Usage: bash scripts/sync_submodules.sh

repo_root="$(cd -- "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

# Path → Commit mapping (do not edit without updating doc/submodule_versions.md)
entries=(
  "src/ref/bcc 8d85dcfac86b"
  "src/ref/libbpf ca72d0731f8c"
  "src/ref/ghostscope 8d6271f2452b"
  "src/ref/parca 279aba38f71b"
)

for entry in "${entries[@]}"; do
  set -- $entry
  path="$1"
  commit="$2"

  echo "==> Updating $path to $commit"
  git submodule update --init --recursive -- "$path"
  git -C "$path" fetch --tags --quiet
  git -C "$path" checkout "$commit"
done

echo "\nSubmodule status after sync:"
git submodule status
