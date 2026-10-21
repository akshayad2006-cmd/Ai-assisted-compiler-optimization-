#!/usr/bin/env bash
exec "$(dirname "$0")/run_stage_tests.sh" --symbols tests/symbol_table
