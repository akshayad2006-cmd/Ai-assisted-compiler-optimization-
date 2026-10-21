#!/usr/bin/env bash
cd "$(dirname "$0")/.." || exit 1
PYTHONPATH=python python3 -m unittest python.tests.test_integration -v
