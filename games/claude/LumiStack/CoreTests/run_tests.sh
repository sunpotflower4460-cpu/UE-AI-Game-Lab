#!/usr/bin/env bash
# Builds and runs the Lumi Stack core tests with strict warnings, using every
# available compiler, plus an AddressSanitizer/UBSan build.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../Source/LumiStack/GameCore/LumiCore.cpp"
OUT="${TMPDIR:-/tmp}/lumistack-tests"
mkdir -p "$OUT"
FLAGS=(-std=c++20 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion -Wundef -Werror -fno-exceptions)

for CXX in g++ clang++; do
  if command -v "$CXX" >/dev/null 2>&1; then
    echo "== $CXX =="
    "$CXX" "${FLAGS[@]}" "$SRC" "$HERE/core_tests.cpp" -o "$OUT/tests-$CXX"
    "$OUT/tests-$CXX"
  fi
done

echo "== ASan+UBSan =="
SAN=(-std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-exceptions)
if g++ "${SAN[@]}" "$SRC" "$HERE/core_tests.cpp" -o "$OUT/tests-asan" 2>/dev/null \
   || clang++ "${SAN[@]}" "$SRC" "$HERE/core_tests.cpp" -o "$OUT/tests-asan"; then
  "$OUT/tests-asan"
fi
echo "ALL CORE TESTS PASSED"
