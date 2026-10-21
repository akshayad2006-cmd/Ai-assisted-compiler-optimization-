"""Builds the text we send to the model.

The model sees ONLY the analysis (candidates, blocked rewrites, program
statistics). It is told to choose candidate ids and to answer in strict JSON.
"""
from __future__ import annotations

import json

SYSTEM_PROMPT = """\
You are an optimization advisor for a small compiler. You do NOT write code and you do NOT change the program. You only choose which of the listed optimization candidates are worth applying.

Rules:
- Choose only candidate ids that appear in the candidate list. Never invent an id.
- A candidate whose "requires" list is not empty is only valid together with the candidates it requires.
- Several candidates can target the same instruction; they are alternatives, so pick at most one of them per instruction.
- "blocked" lists rewrites that are NOT safe. Never recommend them.
- "static_cost_delta" is an estimate from a fixed table, not a measurement. Negative is better.
- If nothing is worth applying, return an empty list.

Reply with ONE JSON object and nothing else (no markdown, no commentary):
{"recommendations": [{"candidate_id": "C1", "reason": "one short sentence"}]}
"""


def build_user_prompt(analysis: dict) -> str:
    """The analysis as a compact message for the model."""
    shown = {
        "program": analysis["program"],
        "instructions": analysis["instructions"],
        "candidates": analysis["candidates"],
        "blocked": analysis["blocked"],
    }
    return ("Here is the analysis of one program.\n"
            "<analysis>\n" + json.dumps(shown, indent=1) + "\n</analysis>\n"
            "Choose which candidates to apply.")


def build_retry_message(problems: list[str]) -> str:
    """Tells the model exactly what was wrong with its last reply."""
    lines = "\n".join("- " + problem for problem in problems)
    return ("Your last reply was rejected:\n" + lines +
            "\nReply again with ONE JSON object in the required format and nothing else.")
