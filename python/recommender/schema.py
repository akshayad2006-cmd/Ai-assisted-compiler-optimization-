"""Strict checking of what the model sends back.

The model must answer with EXACTLY this JSON and nothing else:

    {"recommendations": [{"candidate_id": "C3", "reason": "short text"}]}

This module checks the FORMAT and that every candidate id really exists.
Whether a recommendation is LEGAL (dependencies, conflicts, safety) is the
validator's job in a later milestone, never the model's and never this file's.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass

MAX_REASON_LENGTH = 300
_ID_PATTERN = re.compile(r"^C[1-9][0-9]*$")
_FENCE = re.compile(r"^```(?:json)?[ \t]*\n(.*)\n```[ \t]*$", re.DOTALL)


class SchemaError(ValueError):
    """The reply is not in the required format. 'problems' lists every issue."""

    def __init__(self, problems: list[str]):
        super().__init__("; ".join(problems))
        self.problems = problems


@dataclass(frozen=True)
class Recommendation:
    candidate_id: str
    reason: str


def _reject_duplicate_keys(pairs):
    keys = [key for key, _ in pairs]
    if len(keys) != len(set(keys)):
        raise SchemaError(["the JSON object repeats a key"])
    return dict(pairs)


def parse_model_reply(text: str):
    """Text -> JSON value. Only pure JSON is accepted.

    One leniency: if the WHOLE reply is a single ```json code fence, the
    fence is removed first. Any other text around the JSON is an error.
    """
    stripped = text.strip()
    fenced = _FENCE.match(stripped)
    if fenced:
        stripped = fenced.group(1).strip()
    if not stripped:
        raise SchemaError(["the reply is empty"])
    try:
        return json.loads(stripped, object_pairs_hook=_reject_duplicate_keys)
    except SchemaError:
        raise
    except json.JSONDecodeError as error:
        raise SchemaError(["the reply is not valid JSON (%s); reply with the JSON "
                           "object only, no other text" % error.msg])


def validate_recommendations(data, valid_ids) -> list[Recommendation]:
    """Checks the parsed JSON against the required format.

    Raises SchemaError listing ALL problems found (not just the first), so
    the model can fix everything in one retry.
    """
    problems: list[str] = []
    if not isinstance(data, dict):
        raise SchemaError(["the top level must be a JSON object"])
    if set(data) != {"recommendations"}:
        extra = sorted(set(data) - {"recommendations"})
        if extra:
            problems.append("unexpected top-level key(s): %s" % ", ".join(map(str, extra)))
        if "recommendations" not in data:
            problems.append("missing the key \"recommendations\"")
    items = data.get("recommendations")
    result: list[Recommendation] = []
    if "recommendations" in data and not isinstance(items, list):
        problems.append("\"recommendations\" must be a list")
    elif isinstance(items, list):
        seen: set[str] = set()
        for position, item in enumerate(items, start=1):
            where = "recommendation %d" % position
            if not isinstance(item, dict):
                problems.append("%s must be an object" % where)
                continue
            if set(item) != {"candidate_id", "reason"}:
                problems.append("%s must have exactly the keys candidate_id and reason" % where)
                continue
            cid, reason = item["candidate_id"], item["reason"]
            ok = True
            if not isinstance(cid, str) or not _ID_PATTERN.match(cid):
                problems.append("%s: candidate_id must look like \"C1\"" % where)
                ok = False
            elif cid not in valid_ids:
                problems.append("%s: candidate_id %s is not in the candidate list" % (where, cid))
                ok = False
            elif cid in seen:
                problems.append("%s: candidate_id %s is listed twice" % (where, cid))
                ok = False
            if not isinstance(reason, str) or not reason.strip():
                problems.append("%s: reason must be a non-empty string" % where)
                ok = False
            elif len(reason) > MAX_REASON_LENGTH:
                problems.append("%s: reason is longer than %d characters" % (where, MAX_REASON_LENGTH))
                ok = False
            if ok:
                seen.add(cid)
                result.append(Recommendation(cid, reason.strip()))
    if problems:
        raise SchemaError(problems)
    return result
