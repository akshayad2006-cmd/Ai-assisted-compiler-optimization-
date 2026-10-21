#!/bin/bash

set -u

COMPILER="./build/aicompiler"
PASSED=0
FAILED=0
EMPTY_FILE="$(mktemp)"

cleanup() {
    rm -f "$EMPTY_FILE"
}
trap cleanup EXIT

check_contains() {
    local output="$1"
    local expected="$2"
    local test_name="$3"

    if printf '%s\n' "$output" | grep -Fq -- "$expected"; then
        echo "PASS: $test_name"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: $test_name"
        echo "  Expected to find: $expected"
        FAILED=$((FAILED + 1))
    fi
}

run_performance_test() {
    local source="$1"
    local label="$2"
    shift 2

    local output
    if ! output=$("$COMPILER" --performance "$source" 2>&1); then
        echo "FAIL: $label - compiler command failed"
        printf '%s\n' "$output"
        FAILED=$((FAILED + 1))
        return
    fi

    echo
    echo "Testing: $label"

    while [ "$#" -gt 0 ]; do
        check_contains "$output" "$1" "$label: $1"
        shift
    done

    check_contains "$output" \
        "Runtime speedup has not been measured by this report." \
        "$label: runtime-speedup disclaimer"
}

if [ ! -x "$COMPILER" ]; then
    echo "ERROR: Compiler not found at $COMPILER"
    echo "Run bash scripts/build.sh first."
    exit 1
fi

run_performance_test "examples/demo1.src" "demo1" \
    "Original instruction count : 6" \
    "Optimized instruction count: 1" \
    "Instructions removed      : 5" \
    "Instruction reduction     : 83.33%" \
    "Original estimated cost   : 9" \
    "Optimized estimated cost  : 2" \
    "Estimated cost reduction  : 77.78%" \
    "Optimization changes applied: 11"

run_performance_test "examples/demo2.src" "demo2" \
    "Original instruction count : 15" \
    "Optimized instruction count: 6" \
    "Instructions removed      : 9" \
    "Instruction reduction     : 60.00%" \
    "Original estimated cost   : 29" \
    "Optimized estimated cost  : 18" \
    "Estimated cost reduction  : 37.93%" \
    "Optimization changes applied: 21"

run_performance_test "$EMPTY_FILE" "empty source" \
    "Original instruction count : 0" \
    "Optimized instruction count: 0" \
    "Instructions removed      : 0" \
    "Instruction reduction     : 0.00%" \
    "Original estimated cost   : 0" \
    "Optimized estimated cost  : 0" \
    "Estimated cost reduction  : 0.00%" \
    "Optimization changes applied: 0" \
    "No reduction was recorded by the static metrics for this program."

echo
echo "=================================="
echo "Performance Test Summary"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "=================================="

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi

echo "All performance tests passed."
exit 0
