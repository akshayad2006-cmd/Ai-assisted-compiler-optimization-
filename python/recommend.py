#!/usr/bin/env python3
"""Command line: asks an AI model which optimization candidates to apply.

Examples:
  python3 python/recommend.py --source examples/demo2.src --provider fake:greedy
  python3 python/recommend.py --analysis analysis.json --provider fake:none

Prints ONE JSON result on stdout. Exit codes: 0 ok, 1 no recommendation
(failed), 2 bad usage or bad input.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from recommender.config import load_dotenv  # noqa: E402
from recommender.core import DEFAULT_MAX_ATTEMPTS, AnalysisError, recommend  # noqa: E402
from recommender.providers import ProviderError, make_provider  # noqa: E402


def load_analysis(args) -> dict:
    """Reads the analysis from a JSON file, or runs the compiler to make it."""
    if args.analysis:
        with open(args.analysis, encoding="utf-8") as handle:
            return json.load(handle)
    flag = "--analyze-tac" if args.tac else "--analyze"
    done = subprocess.run([args.compiler, flag, args.source],
                          capture_output=True, text=True)
    if done.returncode != 0:
        raise AnalysisError("the compiler failed: %s" % done.stderr.strip())
    return json.loads(done.stdout)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    where = parser.add_mutually_exclusive_group(required=True)
    where.add_argument("--analysis", help="analysis JSON file (from aicompiler --analyze)")
    where.add_argument("--source", help="source file; the compiler is run for you")
    parser.add_argument("--tac", action="store_true",
                        help="with --source: the file is hand-written TAC text")
    parser.add_argument("--compiler", default="build/aicompiler",
                        help="path to the compiler (default: build/aicompiler)")
    parser.add_argument("--provider", default=None,
                        help="e.g. fake:greedy (default: $AI_PROVIDER)")
    parser.add_argument("--max-attempts", type=int, default=DEFAULT_MAX_ATTEMPTS)
    args = parser.parse_args(argv)

    load_dotenv()
    spec = args.provider or os.environ.get("AI_PROVIDER")
    if not spec:
        print("error: choose a provider with --provider or set AI_PROVIDER", file=sys.stderr)
        return 2
    try:
        provider = make_provider(spec)
        result = recommend(load_analysis(args), provider, args.max_attempts)
    except (ProviderError, AnalysisError, OSError, json.JSONDecodeError) as error:
        print("error: %s" % error, file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "ok" else 1


if __name__ == "__main__":
    sys.exit(main())
