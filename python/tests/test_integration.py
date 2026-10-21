"""End-to-end tests for the AI-assisted compiler integration."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "python" / "optimize_with_ai.py"
COMPILER = ROOT / "build" / "aicompiler"
DEMO_SOURCE = ROOT / "examples" / "demo2.src"


class IntegrationTests(unittest.TestCase):
    def run_pipeline(self, *args):
        return subprocess.run(
            [sys.executable, str(SCRIPT), *map(str, args)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )

    def result_json(self, process):
        self.assertTrue(
            process.stdout.strip(),
            msg=f"No JSON output.\nstdout={process.stdout}\nstderr={process.stderr}",
        )
        try:
            return json.loads(process.stdout)
        except json.JSONDecodeError as exc:
            self.fail(
                f"Output was not valid JSON: {exc}\n"
                f"stdout={process.stdout}\nstderr={process.stderr}"
            )

    def test_greedy_recommendations_are_validated(self):
        process = self.run_pipeline(
            "--source", DEMO_SOURCE, "--provider", "fake:greedy"
        )
        result = self.result_json(process)

        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(result["status"], "ok")
        self.assertFalse(result["recommender"]["is_real_ai"])
        self.assertEqual(
            result["validator"]["summary"]["total_recommendations"],
            len(result["recommender"]["recommendations"]),
        )
        self.assertEqual(result["validator"]["summary"]["rejected"], 0)
        self.assertEqual(result["validator"]["status"], "ok")
        self.assertTrue(result["validator"]["runtime_check"]["equivalent"])
        self.assertIn("print x", result["validator"]["validated_tac"])

    def test_no_recommendations_preserves_original_tac(self):
        process = self.run_pipeline(
            "--source", DEMO_SOURCE, "--provider", "fake:none"
        )
        result = self.result_json(process)

        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["recommender"]["recommendations"], [])
        self.assertEqual(result["validator"]["summary"]["accepted"], 0)
        self.assertEqual(result["validator"]["summary"]["rejected"], 0)
        self.assertIn("t3 = a * 1", result["validator"]["validated_tac"])

    def test_invalid_model_reply_fails_closed(self):
        process = self.run_pipeline(
            "--source", DEMO_SOURCE,
            "--provider", "fake:invalid_json",
            "--max-attempts", "1",
        )
        result = self.result_json(process)

        self.assertNotEqual(process.returncode, 0)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["stage"], "recommender")
        self.assertIsNone(result["validator"])

    def test_unknown_candidate_fails_closed(self):
        process = self.run_pipeline(
            "--source", DEMO_SOURCE,
            "--provider", "fake:unknown_id",
            "--max-attempts", "1",
        )
        result = self.result_json(process)

        self.assertNotEqual(process.returncode, 0)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["stage"], "recommender")
        self.assertIsNone(result["validator"])

    def test_flaky_provider_recovers_on_retry(self):
        process = self.run_pipeline(
            "--source", DEMO_SOURCE,
            "--provider", "fake:flaky",
            "--max-attempts", "2",
        )
        result = self.result_json(process)

        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["recommender"]["attempts"], 2)
        self.assertEqual(result["validator"]["summary"]["rejected"], 0)

    def test_tac_input_uses_tac_pipeline(self):
        generated = subprocess.run(
            [str(COMPILER), "--tac", str(DEMO_SOURCE)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(
            generated.returncode, 0,
            msg=f"Could not generate TAC: {generated.stderr}",
        )
        self.assertTrue(generated.stdout.strip())

        with tempfile.TemporaryDirectory() as temp_dir:
            tac_path = Path(temp_dir) / "demo.tac"
            tac_path.write_text(generated.stdout, encoding="utf-8")

            process = self.run_pipeline(
                "--tac", tac_path, "--provider", "fake:greedy"
            )
            result = self.result_json(process)

        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["input_type"], "tac")
        self.assertEqual(result["validator"]["summary"]["rejected"], 0)

    def test_missing_input_returns_structured_failure(self):
        missing = ROOT / "examples" / "file_that_does_not_exist.src"
        process = self.run_pipeline(
            "--source", missing, "--provider", "fake:greedy"
        )
        result = self.result_json(process)

        self.assertNotEqual(process.returncode, 0)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["stage"], "integration")
        self.assertIn("Input file not found", result["message"])


if __name__ == "__main__":
    unittest.main()
