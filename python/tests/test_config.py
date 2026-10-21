import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from recommender.config import load_dotenv, parse_env_text  # noqa: E402


class ParseEnvTextTest(unittest.TestCase):
    def test_basic_pairs(self):
        self.assertEqual(parse_env_text("A=1\nB=two"), {"A": "1", "B": "two"})

    def test_comments_blank_lines_and_junk_are_ignored(self):
        text = "# a comment\n\nA=1\nnot a pair\n   # indented comment\n"
        self.assertEqual(parse_env_text(text), {"A": "1"})

    def test_quotes_and_export_are_removed(self):
        text = 'export A="hello world"\nB=\'x\'\nC="unbalanced\n'
        self.assertEqual(parse_env_text(text),
                         {"A": "hello world", "B": "x", "C": '"unbalanced'})

    def test_value_may_contain_equals_sign(self):
        self.assertEqual(parse_env_text("KEY=abc=def"), {"KEY": "abc=def"})

    def test_empty_key_is_ignored(self):
        self.assertEqual(parse_env_text("=value"), {})


class LoadDotenvTest(unittest.TestCase):
    def write_env(self, text):
        folder = tempfile.mkdtemp()
        path = os.path.join(folder, ".env")
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(text)
        return path

    def test_loads_new_names_and_returns_names_only(self):
        path = self.write_env("SECRET_KEY=hunter2\n")
        environ = {}
        loaded = load_dotenv(path, environ)
        self.assertEqual(loaded, ["SECRET_KEY"])
        self.assertEqual(environ["SECRET_KEY"], "hunter2")
        self.assertNotIn("hunter2", repr(loaded))

    def test_existing_environment_wins(self):
        path = self.write_env("A=from_file\n")
        environ = {"A": "from_env"}
        self.assertEqual(load_dotenv(path, environ), [])
        self.assertEqual(environ["A"], "from_env")

    def test_missing_file_is_fine(self):
        self.assertEqual(load_dotenv("/no/such/folder/.env", {}), [])


if __name__ == "__main__":
    unittest.main()
