#!/usr/bin/env bash
cd "$(dirname "$0")/.." || exit 1
PYTHONPATH=python python3 -m unittest discover -s python/tests -v 2>&1 | tail -n "${1:-4}"
exit "${PIPESTATUS[0]}"
