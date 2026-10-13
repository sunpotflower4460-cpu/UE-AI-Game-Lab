#!/usr/bin/env bash
# Compiles the shared C++ game core to WebAssembly (needs `pip install ziglang`).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
python3 -m ziglang c++ -target wasm32-wasi -O2 -s -fno-exceptions -mexec-model=reactor -std=c++20 \
  "$HERE/lumi_wasm.cpp" "$HERE/../Source/LumiStack/GameCore/LumiCore.cpp" -o "$HERE/lumi.wasm"
ls -l "$HERE/lumi.wasm"
