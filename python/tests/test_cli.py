import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CLI = os.path.join(ROOT, "python", "recommend.py")
COMPILER = os.path.join(ROOT, "build", "aicompiler")

ANALYSIS = {
    "schema_version": 1,
    "program": {"instruction_count": 3, "static_cost": 6,
                "optimized_instruction_count": 1, "optimized_static_cost": 2},
    "instructions": [{"index": 1, "text": "t1 = 10 * 20"}],
    "candidates": [{"id": "C1", "rule": "constant_folding", "instruction": 1,
                    "before": "t1 = 10 * 20", "after": "t1 = 200",
                    "removes_instruction": False, "static_cost_delta": -2, "requires": []}],
    "blocked": [],
}


def run_cli(*args, cwd, extra_env=None):
    env = {k: v for k, v in os.environ.items() if k != "AI_PROVIDER"}
    env.update(extra_env or {})
    return subprocess.run([sys.executable, CLI, *args], capture_output=True,
                          text=True, cwd=cwd, env=env)


class CliTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.mkdtemp()  # also keeps a real .env out of the tests
        self.path = os.path.join(self.folder, "analysis.json")
        with open(self.path, "w", encoding="utf-8") as handle:
            json.dump(ANALYSIS, handle)

    def test_ok_exit_zero_and_json_output(self):
        done = run_cli("--analysis", self.path, "--provider", "fake:greedy", cwd=self.folder)
        self.assertEqual(done.returncode, 0, done.stderr)
        result = json.loads(done.stdout)
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["recommendations"][0]["candidate_id"], "C1")
        self.assertFalse(result["is_real_ai"])

    def test_failed_recommendation_exits_one(self):
        done = run_cli("--analysis", self.path, "--provider", "fake:invalid_json", cwd=self.folder)
        self.assertEqual(done.returncode, 1)
        self.assertEqual(json.loads(done.stdout)["status"], "failed")

    def test_provider_from_environment(self):
        done = run_cli("--analysis", self.path, cwd=self.folder,
                       extra_env={"AI_PROVIDER": "fake:none"})
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(json.loads(done.stdout)["provider"], "fake:none")

    def test_provider_from_dotenv_file(self):
        with open(os.path.join(self.folder, ".env"), "w", encoding="utf-8") as handle:
            handle.write("AI_PROVIDER=fake:none\n")
        done = run_cli("--analysis", self.path, cwd=self.folder)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(json.loads(done.stdout)["provider"], "fake:none")

    def test_no_provider_is_a_usage_error(self):
        done = run_cli("--analysis", self.path, cwd=self.folder)
        self.assertEqual(done.returncode, 2)
        self.assertIn("provider", done.stderr)

    def test_unavailable_provider_is_a_usage_error(self):
        done = run_cli("--analysis", self.path, "--provider", "anthropic", cwd=self.folder)
        self.assertEqual(done.returncode, 2)
        self.assertIn("not available yet", done.stderr)

    def test_missing_and_broken_files_are_usage_errors(self):
        missing = run_cli("--analysis", os.path.join(self.folder, "nope.json"),
                          "--provider", "fake", cwd=self.folder)
        self.assertEqual(missing.returncode, 2)
        broken = os.path.join(self.folder, "broken.json")
        with open(broken, "w", encoding="utf-8") as handle:
            handle.write("{not json")
        self.assertEqual(run_cli("--analysis", broken, "--provider", "fake",
                                 cwd=self.folder).returncode, 2)

    def test_wrong_schema_version_is_a_usage_error(self):
        bad = os.path.join(self.folder, "bad.json")
        with open(bad, "w", encoding="utf-8") as handle:
            json.dump(dict(ANALYSIS, schema_version=7), handle)
        done = run_cli("--analysis", bad, "--provider", "fake", cwd=self.folder)
        self.assertEqual(done.returncode, 2)
        self.assertIn("schema_version", done.stderr)


@unittest.skipUnless(os.path.exists(COMPILER), "build the compiler first (scripts/build.sh)")
class EndToEndTest(unittest.TestCase):
    def test_source_file_through_real_compiler(self):
        done = run_cli("--source", os.path.join(ROOT, "examples", "demo2.src"),
                       "--compiler", COMPILER, "--provider", "fake:greedy", cwd=ROOT)
        self.assertEqual(done.returncode, 0, done.stderr)
        result = json.loads(done.stdout)
        self.assertEqual([r["candidate_id"] for r in result["recommendations"]],
                         ["C1", "C8", "C9"])

    def test_compiler_error_is_reported(self):
        folder = tempfile.mkdtemp()
        bad = os.path.join(folder, "bad.src")
        with open(bad, "w", encoding="utf-8") as handle:
            handle.write("int x = 10 +;\n")
        done = run_cli("--source", bad, "--compiler", COMPILER, "--provider", "fake", cwd=folder)
        self.assertEqual(done.returncode, 2)
        self.assertIn("compiler failed", done.stderr)

    def test_tac_source(self):
        folder = tempfile.mkdtemp()
        tac = os.path.join(folder, "p.tac")
        with open(tac, "w", encoding="utf-8") as handle:
            handle.write("a = input()\nt1 = a * 2\nt2 = t1 + 1\nprint a\n")
        done = run_cli("--source", tac, "--tac", "--compiler", COMPILER,
                       "--provider", "fake:greedy", cwd=folder)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual([r["candidate_id"] for r in json.loads(done.stdout)["recommendations"]],
                         ["C1", "C2"])


if __name__ == "__main__":
    unittest.main()
