import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from recommender.core import AnalysisError, check_analysis, recommend  # noqa: E402
from recommender.providers import (FAKE_MODES, FakeProvider, Provider,  # noqa: E402
                                   ProviderError, make_provider)


def candidate(cid, delta, rule="dead_code_elimination", instruction=1):
    return {"id": cid, "rule": rule, "instruction": instruction, "before": "x = 1",
            "after": "(removed)", "removes_instruction": delta < 0,
            "static_cost_delta": delta, "requires": []}


def analysis(candidates):
    return {"schema_version": 1,
            "program": {"instruction_count": 3, "static_cost": 6,
                        "optimized_instruction_count": 1, "optimized_static_cost": 2},
            "instructions": [{"index": 1, "text": "x = 1"}],
            "candidates": candidates, "blocked": []}


THREE = analysis([candidate("C1", -2), candidate("C2", 0), candidate("C3", -1)])


class CheckAnalysisTest(unittest.TestCase):
    def test_accepts_valid(self):
        check_analysis(THREE)

    def test_rejects_wrong_shapes(self):
        for bad in ([], {"schema_version": 2}, {"schema_version": 1},
                    dict(THREE, candidates="x")):
            with self.assertRaises(AnalysisError):
                check_analysis(bad)


class RecommendTest(unittest.TestCase):
    def ids(self, result):
        return [r["candidate_id"] for r in result["recommendations"]]

    def test_greedy_picks_negative_deltas(self):
        result = recommend(THREE, FakeProvider("greedy"))
        self.assertEqual(result["status"], "ok")
        self.assertEqual(self.ids(result), ["C1", "C3"])
        self.assertEqual(result["attempts"], 1)
        self.assertEqual(result["errors"], [])

    def test_result_says_it_is_not_real_ai(self):
        result = recommend(THREE, FakeProvider("greedy"))
        self.assertFalse(result["is_real_ai"])
        self.assertEqual(result["provider"], "fake:greedy")

    def test_empty_recommendation_is_ok(self):
        result = recommend(THREE, FakeProvider("none"))
        self.assertEqual((result["status"], self.ids(result)), ("ok", []))

    def test_fenced_reply_is_accepted(self):
        result = recommend(THREE, FakeProvider("fenced"))
        self.assertEqual((result["status"], self.ids(result)), ("ok", ["C1", "C3"]))

    def test_bad_modes_fail_after_all_attempts_with_no_recommendations(self):
        for mode in ("invalid_json", "extra_field", "unknown_id", "duplicate"):
            provider = FakeProvider(mode)
            result = recommend(THREE, provider, max_attempts=3)
            self.assertEqual(result["status"], "failed", mode)
            self.assertEqual(result["recommendations"], [], mode)
            self.assertEqual(provider.calls, 3, mode)
            self.assertEqual(result["attempts"], 3, mode)
            self.assertTrue(result["errors"], mode)

    def test_flaky_recovers_on_second_attempt(self):
        provider = FakeProvider("flaky")
        result = recommend(THREE, provider)
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["attempts"], 2)
        self.assertEqual(self.ids(result), ["C1", "C3"])
        self.assertTrue(result["errors"])  # the rejected first reply is noted

    def test_retry_message_tells_the_model_what_was_wrong(self):
        seen = []

        class Spy(FakeProvider):
            def complete(self, system, messages):
                seen.append([m["content"] for m in messages])
                return super().complete(system, messages)

        recommend(THREE, Spy("flaky"))
        self.assertEqual(len(seen), 2)
        self.assertEqual(len(seen[1]), 3)  # prompt, bad reply, correction
        self.assertIn("rejected", seen[1][2])
        self.assertIn("not valid JSON", seen[1][2])

    def test_no_candidates_means_no_model_call(self):
        provider = FakeProvider("greedy")
        result = recommend(analysis([]), provider)
        self.assertEqual((result["status"], result["attempts"], provider.calls), ("ok", 0, 0))

    def test_provider_error_fails_without_retrying(self):
        class Broken(Provider):
            name = "broken"
            calls = 0

            def complete(self, system, messages):
                Broken.calls += 1
                raise ProviderError("no API key")

        result = recommend(THREE, Broken())
        self.assertEqual(result["status"], "failed")
        self.assertEqual(Broken.calls, 1)
        self.assertIn("no API key", result["errors"][0])

    def test_prompt_contains_only_the_analysis(self):
        seen = []

        class Spy(FakeProvider):
            def complete(self, system, messages):
                seen.append((system, messages[0]["content"]))
                return super().complete(system, messages)

        recommend(THREE, Spy("greedy"))
        system, user = seen[0]
        self.assertIn("<analysis>", user)
        self.assertIn("C3", user)
        self.assertIn("Choose only candidate ids", system)


class ProviderFactoryTest(unittest.TestCase):
    def test_fake_modes_exist(self):
        for mode in FAKE_MODES:
            self.assertEqual(make_provider("fake:" + mode).name, "fake:" + mode)

    def test_fake_defaults_to_greedy(self):
        self.assertEqual(make_provider("fake").name, "fake:greedy")

    def test_unknown_mode_and_provider_are_errors(self):
        for spec in ("fake:nonsense", "anthropic", "openai:gpt"):
            with self.assertRaises(ProviderError):
                make_provider(spec)


if __name__ == "__main__":
    unittest.main()
