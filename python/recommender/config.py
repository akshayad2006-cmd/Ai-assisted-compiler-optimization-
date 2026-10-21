"""Settings from the environment and from a local .env file.

Secrets (API keys) live ONLY in .env, which git ignores. This module never
prints or returns a secret's value - only the NAMES of what it loaded.
"""
from __future__ import annotations

import os
from typing import MutableMapping


def parse_env_text(text: str) -> dict[str, str]:
    """Parses the text of a .env file: lines like KEY=value.

    Blank lines and lines starting with '#' are ignored. A leading 'export '
    is allowed. One pair of matching quotes around the value is removed.
    """
    values: dict[str, str] = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[len("export "):].lstrip()
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        if key:
            values[key] = value
    return values


def load_dotenv(path: str = ".env",
                environ: MutableMapping[str, str] | None = None) -> list[str]:
    """Loads KEY=value pairs from 'path' into the environment.

    Variables that are already set are NOT overridden (a real environment
    variable beats the file). Returns the names that were set; a missing
    file is fine and returns an empty list.
    """
    target = os.environ if environ is None else environ
    try:
        with open(path, encoding="utf-8") as handle:
            pairs = parse_env_text(handle.read())
    except FileNotFoundError:
        return []
    loaded = []
    for key, value in pairs.items():
        if key not in target:
            target[key] = value
            loaded.append(key)
    return loaded
