#!/usr/bin/env bash
# Generic test runner for one compiler stage.
# Usage: scripts/run_stage_tests.sh <compiler-flag> <test-directory>
#   valid_*.src -> exit code must be 0 and stdout must equal valid_*.expected
#   error_*.src -> exit code must be 1 and stderr must contain the text
#                  from error_*.expected
cd "$(dirname "$0")/.." || exit 1

FLAG="$1"
DIR="$2"
BIN=build/aicompiler
pass=0
fail=0

for src in "$DIR"/valid_*.src; do
    [ -e "$src" ] || continue
    expected="${src%.src}.expected"
    actual="$("$BIN" "$FLAG" "$src" 2>/dev/null)"
    code=$?
    if [ "$code" -eq 0 ] && [ "$actual" = "$(cat "$expected")" ]; then
        echo "PASS  $src"
        pass=$((pass + 1))
    else
        echo "FAIL  $src (exit code $code)"
        fail=$((fail + 1))
    fi
done

for src in "$DIR"/error_*.src; do
    [ -e "$src" ] || continue
    expected="${src%.src}.expected"
    needle="$(cat "$expected")"
    output="$("$BIN" "$FLAG" "$src" 2>&1 > /dev/null)"
    code=$?
    if [ "$code" -eq 1 ] && printf '%s' "$output" | grep -qF -- "$needle"; then
        echo "PASS  $src"
        pass=$((pass + 1))
    else
        echo "FAIL  $src (exit code $code)"
        echo "      stderr was: $output"
        fail=$((fail + 1))
    fi
done

echo "---- $pass passed, $fail failed ----"
[ "$fail" -eq 0 ]
