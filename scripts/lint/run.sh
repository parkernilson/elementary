#!/usr/bin/env bash
#
# Runs inside the elem-lint docker image (see docker/lint/Dockerfile). Not meant
# to be run directly on the host - use scripts/lint.sh instead.
set -euo pipefail

ROOT_DIR="/src"
BUILD_DIR="/tmp/elem-lint-build"

FIX=0
if [[ "${1:-}" == "--fix" ]]; then
    FIX=1
fi

mapfile -t FILES < <(find "$ROOT_DIR/runtime" "$ROOT_DIR/cli" "$ROOT_DIR/tests" \
    \( -name "*.cpp" -o -name "*.cc" -o -name "*.h" -o -name "*.hpp" \) \
    -not -path "*/third-party/*" \
    -not -path "*/deps/*" \
    -not -path "*/choc/*" \
    -not -name "miniaudio.h")

echo "==> clang-format (${#FILES[@]} files)"
if [[ "$FIX" -eq 1 ]]; then
    clang-format-18 -i "${FILES[@]}"
else
    clang-format-18 --dry-run --Werror "${FILES[@]}"
fi

echo "==> configuring compile database for clang-tidy"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_CXX_COMPILER=clang++-18 \
    -DCMAKE_C_COMPILER=clang-18 \
    > /dev/null

TU_FILES=()
for f in "${FILES[@]}"; do
    case "$f" in
        *.cpp|*.cc) TU_FILES+=("$f") ;;
    esac
done

echo "==> clang-tidy (${#TU_FILES[@]} translation units)"
if [[ "$FIX" -eq 1 ]]; then
    clang-tidy-18 -p "$BUILD_DIR" --fix "${TU_FILES[@]}"
else
    clang-tidy-18 -p "$BUILD_DIR" "${TU_FILES[@]}"
fi
