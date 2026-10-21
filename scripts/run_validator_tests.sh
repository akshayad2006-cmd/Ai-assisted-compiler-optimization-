#!/usr/bin/env bash
set -u

cd "$(dirname "$0")/.." || exit 1

BIN="build/aicompiler"
TMP_DIR="$(mktemp -d)"
pass=0
fail=0

trap 'rm -rf "$TMP_DIR"' EXIT

if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not found. Run scripts/build.sh first."
    exit 1
fi

SOURCE="tests/analysis/valid_fold.src"

cat > "$TMP_DIR/test.tac" <<'TAC'
t1 = 10 * 20
x = t1
print x
TAC

cat > "$TMP_DIR/valid.json" <<'JSON'
{
  "recommendations": [{
    "candidate_id": "C1",
    "rule": "constant_folding",
    "instruction": 1,
    "before": "t1 = 10 * 20",
    "after": "t1 = 200"
  }]
}
JSON

cat > "$TMP_DIR/invalid.json" <<'JSON'
{
  "recommendations": [{
    "candidate_id": "C1",
    "rule": "constant_folding",
    "instruction": 1,
    "before": "t1 = 10 * 20",
    "after": "t1 = 999"
  }]
}
JSON

cat > "$TMP_DIR/unknown.json" <<'JSON'
{
  "recommendations": [{
    "candidate_id": "C999",
    "rule": "constant_folding",
    "instruction": 1,
    "before": "t1 = 10 * 20",
    "after": "t1 = 200"
  }]
}
JSON

cat > "$TMP_DIR/duplicate.json" <<'JSON'
{
  "recommendations": [
    {
      "candidate_id": "C1",
      "rule": "constant_folding",
      "instruction": 1,
      "before": "t1 = 10 * 20",
      "after": "t1 = 200"
    },
    {
      "candidate_id": "C1",
      "rule": "constant_folding",
      "instruction": 1,
      "before": "t1 = 10 * 20",
      "after": "t1 = 200"
    }
  ]
}
JSON

cat > "$TMP_DIR/malformed.json" <<'JSON'
{
  "recommendations": [{
    "candidate_id": "C1"
JSON

check_case() {
    label="$1"
    expected="$2"
    shift 2

    "$@" > "$TMP_DIR/stdout" 2> "$TMP_DIR/stderr"
    actual=$?

    if [ "$actual" -eq "$expected" ]; then
        echo "PASS  $label (exit $actual)"
        pass=$((pass + 1))
    else
        echo "FAIL  $label (expected exit $expected, got $actual)"
        cat "$TMP_DIR/stderr"
        fail=$((fail + 1))
    fi
}

check_contains() {
    label="$1"
    file="$2"
    expected="$3"

    if grep -qF -- "$expected" "$file"; then
        echo "PASS  $label"
        pass=$((pass + 1))
    else
        echo "FAIL  $label (missing: $expected)"
        fail=$((fail + 1))
    fi
}

echo "Running validator tests..."

check_case "Source accepts approved recommendation" 0 \
    "$BIN" --validate "$SOURCE" "$TMP_DIR/valid.json"
check_contains "Source accepted decision" "$TMP_DIR/stdout" '"status": "accepted"'

check_case "Source rejects unsafe rewrite" 1 \
    "$BIN" --validate "$SOURCE" "$TMP_DIR/invalid.json"
check_contains "Source rejected decision" "$TMP_DIR/stdout" '"status": "rejected"'

check_case "Source rejects unknown candidate" 1 \
    "$BIN" --validate "$SOURCE" "$TMP_DIR/unknown.json"
check_contains "Unknown candidate reason" "$TMP_DIR/stdout" 'Unknown candidate ID'

check_case "Source rejects duplicate candidate" 1 \
    "$BIN" --validate "$SOURCE" "$TMP_DIR/duplicate.json"
check_contains "Duplicate candidate reason" "$TMP_DIR/stdout" 'Duplicate candidate ID'

check_case "Source rejects malformed JSON" 1 \
    "$BIN" --validate "$SOURCE" "$TMP_DIR/malformed.json"
check_contains "Malformed JSON status" "$TMP_DIR/stdout" '"status": "invalid_json"'

check_case "TAC accepts approved recommendation" 0 \
    "$BIN" --validate-tac "$TMP_DIR/test.tac" "$TMP_DIR/valid.json"
check_contains "TAC accepted decision" "$TMP_DIR/stdout" '"status": "accepted"'

check_case "TAC rejects unsafe rewrite" 1 \
    "$BIN" --validate-tac "$TMP_DIR/test.tac" "$TMP_DIR/invalid.json"
check_contains "TAC rejected decision" "$TMP_DIR/stdout" '"status": "rejected"'

echo "---- $pass passed, $fail failed ----"
[ "$fail" -eq 0 ]
