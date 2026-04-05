#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format is required but was not found"
  exit 1
fi

base_sha="${BASE_SHA:-}"
if [[ -z "${base_sha}" || "${base_sha}" =~ ^0+$ ]]; then
  if git rev-parse --verify HEAD~1 >/dev/null 2>&1; then
    base_sha="$(git rev-parse HEAD~1)"
  else
    echo "No base revision found; skipping style check"
    exit 0
  fi
fi

mapfile -t changed_files < <(
  git diff --name-only --diff-filter=ACMRT "${base_sha}...HEAD" -- '*.c' '*.h' '*.cpp' '*.hpp'
)

if [[ ${#changed_files[@]} -eq 0 ]]; then
  echo "No changed C/C++ files to check"
  exit 0
fi

echo "Checking clang-format on changed files"
printf ' - %s\n' "${changed_files[@]}"

clang-format --dry-run --Werror "${changed_files[@]}"
echo "clang-format check passed"
