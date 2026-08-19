#!/usr/bin/env bash
#
# Lints and formats all C++ files in the repo using a pinned clang-format/clang-tidy
# toolchain, run inside Docker so results are identical regardless of what's installed
# on the host.
#
# Usage:
#   ./scripts/lint.sh          # check only, exits non-zero on violations
#   ./scripts/lint.sh --fix    # reformat and auto-fix in place
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
IMAGE_TAG="elem-lint:latest"

docker build -t "$IMAGE_TAG" -f "$ROOT_DIR/docker/lint/Dockerfile" "$ROOT_DIR/docker/lint"

docker run --rm \
    -v "$ROOT_DIR:/src" \
    -w /src \
    "$IMAGE_TAG" \
    /src/scripts/lint/run.sh "$@"
