#!/usr/bin/env python3
"""End-to-end pipeline: compiler analysis -> recommender -> C++ validator."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_COMPILER = ROOT / "build" / "aicompiler"
RECOMMENDER = ROOT / "python" / "recommend.py"


class IntegrationError(RuntimeError):
    """Raised when a pipeline stage cannot produce valid output."""


def run_command(command: list[str]) -> subprocess.CompletedProcess[str]:
    """Run a command without a shell and preserve its stdout/stderr."""
    try:
        return subprocess.run(
            command,
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
    except OSError as exc:
        raise IntegrationError(
            f"Could not start command {command[0]!r}: {exc}"
        ) from exc


def parse_json_output(stdout: str, stage: str) -> dict:
    try:
        value = json.loads(stdout)
    except json.JSONDecodeError as exc:
        raise IntegrationError(
            f"{stage} did not return valid JSON: {exc}"
        ) from exc

    if not isinstance(value, dict):
        raise IntegrationError(f"{stage} returned JSON that is not an object.")

    return value


def run_pipeline(args: argparse.Namespace) -> tuple[int, dict]:
    compiler = Path(args.compiler).expanduser()
    if not compiler.is_absolute():
        compiler = (ROOT / compiler).resolve()

    if not compiler.is_file():
        raise IntegrationError(
            f"Compiler executable not found: {compiler}. "
            "Build the project first with bash scripts/build.sh."
        )

    input_path = Path(args.source or args.tac).expanduser()
    if not input_path.is_absolute():
        input_path = (Path.cwd() / input_path).resolve()

    if not input_path.is_file():
        raise IntegrationError(f"Input file not found: {input_path}")

    is_tac = args.tac is not None
    analysis_flag = "--analyze-tac" if is_tac else "--analyze"
    validate_flag = "--validate-tac" if is_tac else "--validate"

    # Stage 1: the compiler creates the authoritative candidate list.
    analysis_process = run_command(
        [str(compiler), analysis_flag, str(input_path)]
    )
    if analysis_process.returncode != 0:
        message = analysis_process.stderr.strip() or analysis_process.stdout.strip()
        raise IntegrationError(
            "Compiler analysis failed"
            + (f": {message}" if message else ".")
        )

    analysis = parse_json_output(analysis_process.stdout, "Compiler analysis")

    if analysis.get("schema_version") != 1:
        raise IntegrationError("Unsupported compiler analysis schema version.")

    candidates = analysis.get("candidates")
    if not isinstance(candidates, list):
        raise IntegrationError("Analysis JSON is missing its candidates array.")

    candidates_by_id = {}
    for candidate in candidates:
        if not isinstance(candidate, dict):
            raise IntegrationError("Analysis contains a malformed candidate.")
        candidate_id = candidate.get("id")
        if not isinstance(candidate_id, str) or not candidate_id:
            raise IntegrationError("Analysis contains a candidate without an ID.")
        if candidate_id in candidates_by_id:
            raise IntegrationError(
                f"Analysis contains duplicate candidate ID {candidate_id!r}."
            )
        candidates_by_id[candidate_id] = candidate

    with tempfile.TemporaryDirectory(prefix="ai-compiler-integration-") as temp:
        temp_dir = Path(temp)
        analysis_file = temp_dir / "analysis.json"
        recommendations_file = temp_dir / "recommendations.json"

        analysis_file.write_text(
            json.dumps(analysis, ensure_ascii=False), encoding="utf-8"
        )

        # Stage 2: the recommender may select IDs and explain its choices.
        # It does not supply trusted rewrite metadata.
        recommend_command = [
            sys.executable,
            str(RECOMMENDER),
            "--analysis",
            str(analysis_file),
            "--max-attempts",
            str(args.max_attempts),
        ]
        if args.provider:
            recommend_command.extend(["--provider", args.provider])

        recommend_process = run_command(recommend_command)
        try:
            recommendation_result = parse_json_output(
                recommend_process.stdout, "Python recommender"
            )
        except IntegrationError as exc:
            detail = recommend_process.stderr.strip()
            raise IntegrationError(
                f"{exc}"
                + (f"\nRecommender stderr: {detail}" if detail else "")
            ) from exc

        if (
            recommend_process.returncode != 0
            or recommendation_result.get("status") != "ok"
        ):
            return 1, {
                "status": "failed",
                "stage": "recommender",
                "analysis_summary": analysis.get("program", {}),
                "recommender": recommendation_result,
                "validator": None,
                "message": "Recommendations were not validated because the recommender failed.",
            }

        selected = recommendation_result.get("recommendations")
        if not isinstance(selected, list):
            raise IntegrationError(
                "Recommender result is missing its recommendations array."
            )

        # Stage 3: enrich each selected ID from compiler-owned analysis.
        # Never accept rule/before/after/instruction values from the model.
        validator_items = []
        seen_ids = set()

        for item in selected:
            if not isinstance(item, dict):
                raise IntegrationError("Recommender returned a malformed item.")

            candidate_id = item.get("candidate_id")
            reason = item.get("reason")

            if not isinstance(candidate_id, str) or candidate_id not in candidates_by_id:
                raise IntegrationError(
                    f"Recommender selected unknown candidate ID: {candidate_id!r}"
                )
            if candidate_id in seen_ids:
                raise IntegrationError(
                    f"Recommender returned duplicate candidate ID: {candidate_id}"
                )
            if not isinstance(reason, str) or not reason.strip():
                raise IntegrationError(
                    f"Candidate {candidate_id} has no valid explanation."
                )

            seen_ids.add(candidate_id)
            candidate = candidates_by_id[candidate_id]

            validator_items.append({
                "candidate_id": candidate_id,
                "reason": reason,
                "rule": candidate["rule"],
                "instruction": candidate["instruction"],
                "before": candidate["before"],
                "after": candidate["after"],
            })

        recommendations_file.write_text(
            json.dumps({"recommendations": validator_items}, ensure_ascii=False),
            encoding="utf-8",
        )

        # Stage 4: the existing C++ validator makes the final decision.
        validator_process = run_command([
            str(compiler),
            validate_flag,
            str(input_path),
            str(recommendations_file),
        ])

        try:
            validator_report = parse_json_output(
                validator_process.stdout, "C++ validator"
            )
        except IntegrationError as exc:
            detail = validator_process.stderr.strip()
            raise IntegrationError(
                f"{exc}"
                + (f"\nValidator stderr: {detail}" if detail else "")
            ) from exc

        success = (
            validator_process.returncode == 0
            and validator_report.get("status") == "ok"
        )

        return (0 if success else 1), {
            "status": "ok" if success else "failed",
            "input_type": "tac" if is_tac else "source",
            "input_file": str(input_path),
            "analysis_summary": analysis.get("program", {}),
            "recommender": recommendation_result,
            "validator": validator_report,
            "message": (
                "Recommendations were checked by the C++ validator."
                if success
                else "The validator reported a failure or rejected recommendation(s)."
            ),
        }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run compiler analysis, AI-style candidate selection, "
            "and C++ validation as one pipeline."
        )
    )
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--source", help="Path to a source program.")
    inputs.add_argument("--tac", help="Path to an existing TAC file.")
    parser.add_argument(
        "--compiler",
        default=str(DEFAULT_COMPILER),
        help="Path to the compiled aicompiler executable.",
    )
    parser.add_argument(
        "--provider",
        default=os.environ.get("AI_PROVIDER"),
        help="Provider, e.g. fake:greedy or fake:none. Defaults to AI_PROVIDER/.env.",
    )
    parser.add_argument(
        "--max-attempts",
        type=int,
        default=3,
        help="Maximum recommender attempts (default: 3).",
    )

    args = parser.parse_args()

    if args.max_attempts < 1:
        parser.error("--max-attempts must be at least 1")

    try:
        exit_code, result = run_pipeline(args)
    except IntegrationError as exc:
        result = {
            "status": "failed",
            "stage": "integration",
            "message": str(exc),
        }
        exit_code = 1

    print(json.dumps(result, indent=2, ensure_ascii=False))
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
