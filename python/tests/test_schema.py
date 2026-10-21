import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from recommender.schema import (SchemaError, parse_model_reply,  # noqa: E402
                                validate_recommendations)

IDS = {"C1", "C2", "C3"}


def good(*ids):
    return {"recommendations": [{"candidate_id": i, "reason": "because"} for i in ids]}


class ParseTest(unittest.TestCase):
    def test_pure_json(self):
        self.assertEqual(parse_model_reply('{"recommendations": []}'), {"recommendations": []})

    def test_single_code_fence_is_accepted(self):
        text = '```json\n{"recommendations": []}\n```'
        self.assertEqual(parse_model_reply(text), {"recommendations": []})

    def test_text_around_json_is_rejected(self):
        with self.assertRaises(SchemaError):
            parse_model_reply('Here you go: {"recommendations": []}')

    def test_empty_reply_is_rejected(self):
        with self.assertRaises(SchemaError):
            parse_model_reply("   \n ")

    def test_invalid_json_is_rejected(self):
        with self.assertRaises(SchemaError):
            parse_model_reply('{"recommendations": [}')

    def test_duplicate_keys_are_rejected(self):
        with self.assertRaises(SchemaError):
            parse_model_reply('{"recommendations": [], "recommendations": []}')


class ValidateTest(unittest.TestCase):
    def problems(self, data):
        with self.assertRaises(SchemaError) as caught:
            validate_recommendations(data, IDS)
        return caught.exception.problems

    def test_valid_reply(self):
        result = validate_recommendations(good("C1", "C3"), IDS)
        self.assertEqual([r.candidate_id for r in result], ["C1", "C3"])

    def test_empty_list_is_valid(self):
        self.assertEqual(validate_recommendations(good(), IDS), [])

    def test_top_level_must_be_object(self):
        self.assertTrue(self.problems([]))

    def test_missing_key(self):
        self.assertTrue(any("missing" in p for p in self.problems({})))

    def test_extra_top_level_key(self):
        data = good("C1")
        data["confidence"] = 1
        self.assertTrue(any("unexpected" in p for p in self.problems(data)))

    def test_recommendations_must_be_list(self):
        self.assertTrue(any("list" in p for p in self.problems({"recommendations": "C1"})))

    def test_bad_id_format(self):
        data = {"recommendations": [{"candidate_id": "first", "reason": "x"}]}
        self.assertTrue(any("look like" in p for p in self.problems(data)))

    def test_leading_zero_id_is_bad_format(self):
        data = {"recommendations": [{"candidate_id": "C01", "reason": "x"}]}
        self.assertTrue(self.problems(data))

    def test_unknown_id(self):
        self.assertTrue(any("not in the candidate list" in p for p in self.problems(good("C9"))))

    def test_duplicate_id(self):
        self.assertTrue(any("twice" in p for p in self.problems(good("C1", "C1"))))

    def test_item_with_extra_key(self):
        data = {"recommendations": [{"candidate_id": "C1", "reason": "x", "score": 1}]}
        self.assertTrue(any("exactly the keys" in p for p in self.problems(data)))

    def test_item_missing_reason(self):
        data = {"recommendations": [{"candidate_id": "C1"}]}
        self.assertTrue(self.problems(data))

    def test_empty_and_non_string_reason(self):
        for reason in ("  ", 5, None):
            data = {"recommendations": [{"candidate_id": "C1", "reason": reason}]}
            self.assertTrue(self.problems(data), reason)

    def test_reason_too_long(self):
        data = {"recommendations": [{"candidate_id": "C1", "reason": "x" * 301}]}
        self.assertTrue(any("longer" in p for p in self.problems(data)))

    def test_item_must_be_object(self):
        self.assertTrue(self.problems({"recommendations": ["C1"]}))

    def test_all_problems_are_reported_together(self):
        data = {"recommendations": [
            {"candidate_id": "C9", "reason": "x"},
            {"candidate_id": "C1", "reason": ""},
        ]}
        self.assertEqual(len(self.problems(data)), 2)

    def test_non_string_id_does_not_crash(self):
        data = {"recommendations": [{"candidate_id": ["C1"], "reason": "x"}]}
        self.assertTrue(self.problems(data))


if __name__ == "__main__":
    unittest.main()
