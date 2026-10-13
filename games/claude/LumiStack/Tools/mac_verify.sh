#!/usr/bin/env bash
# One-shot verification loop for macOS (designed so an AI agent can run it unattended):
#   1. core unit tests          2. compile the editor target
#   3. in-engine automation tests  4. boot the real game with the autopilot and
#      check that it reaches the summit (LUMI_RESULT phase=Won in the log).
#
# Usage: UE_ROOT="/Users/Shared/Epic Games/UE_5.5" ./Tools/mac_verify.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.5}"
PROJECT="$HERE/LumiStack.uproject"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
EDITOR_CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
LOG_DIR="$HERE/Saved/VerifyLogs"
mkdir -p "$LOG_DIR"

echo "== 1/4 core unit tests"
"$HERE/CoreTests/run_tests.sh" | tail -2

echo "== 2/4 build LumiStackEditor (Mac Development)"
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" LumiStackEditor Mac Development -project="$PROJECT" -waitmutex 2>&1 | tee "$LOG_DIR/build.log" | tail -20

echo "== 3/4 automation tests"
"$EDITOR_CMD" "$PROJECT" -ExecCmds="Automation RunTests LumiStack; Quit" -unattended -nullrhi -nosplash -nopause \
  -log="$LOG_DIR/automation.log" >/dev/null 2>&1 || true
grep -E "Test Completed|Result=\{(Success|Fail)" "$LOG_DIR/automation.log" || true
if grep -q "Result={Fail" "$LOG_DIR/automation.log"; then echo "automation tests FAILED"; exit 1; fi

echo "== 4/4 autoplay run in the real game (portrait window, ~2-3 min)"
GAME_LOG="$LOG_DIR/autoplay.log"
rm -f "$GAME_LOG"
"$EDITOR" "$PROJECT" -game -windowed -ResX=430 -ResY=932 -nosplash -unattended \
  -ExecCmds="lumi.AutoPlay 2" -log="$GAME_LOG" &
GAME_PID=$!
for _ in $(seq 1 360); do
  sleep 1
  if [ -f "$GAME_LOG" ] && grep -q "LUMI_RESULT" "$GAME_LOG"; then break; fi
done
sleep 2
kill "$GAME_PID" 2>/dev/null || true
grep -E "LogLumiStack|Error" "$GAME_LOG" | tail -25 || true
if grep -q "LUMI_RESULT phase=Won" "$GAME_LOG"; then
  echo "PASS: the game booted and the autopilot reached the summit"
else
  echo "FAIL: no win result in $GAME_LOG"
  exit 1
fi
