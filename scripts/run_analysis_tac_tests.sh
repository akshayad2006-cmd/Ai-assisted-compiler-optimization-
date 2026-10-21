#!/usr/bin/env bash
exec "$(dirname "$0")/run_stage_tests.sh" --analyze-tac tests/analysis_tac
