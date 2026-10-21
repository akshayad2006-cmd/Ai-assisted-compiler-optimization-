"""Model providers: the only place that talks to a language model.

A provider has one job: take the prompt, return the model's reply as text.
Real providers (Anthropic, OpenAI, ...) will be added here later.

FakeProvider is a deterministic stand-in for tests. It is NOT an AI and its
output is never to be reported as an AI result: results carry
"is_real_ai": false so later stages can refuse them.
"""
from __future__ import annotations

import json
import re


class ProviderError(RuntimeError):
    """The provider could not produce a reply (no key, network error, ...)."""


class Provider:
    name = "provider"
    is_real_ai = False

    def complete(self, system: str, messages: list[dict]) -> str:
        """Returns the model's reply text. 'messages' alternate user/assistant."""
        raise NotImplementedError


FAKE_MODES = ("greedy", "none", "invalid_json", "fenced", "extra_field",
              "unknown_id", "duplicate", "flaky")


class FakeProvider(Provider):
    """Deterministic fake model for tests. Modes:

    greedy        picks every candidate with a negative static cost delta
    none          recommends nothing
    invalid_json  replies with prose (not JSON)
    fenced        valid JSON wrapped in a ```json fence
    extra_field   valid JSON plus a forbidden extra key
    unknown_id    recommends an id that does not exist
    duplicate     recommends the same id twice
    flaky         first reply is prose, later replies are 'greedy'
    """

    is_real_ai = False

    def __init__(self, mode: str = "greedy"):
        if mode not in FAKE_MODES:
            raise ProviderError("unknown fake mode '%s' (choose from: %s)"
                                % (mode, ", ".join(FAKE_MODES)))
        self.mode = mode
        self.name = "fake:" + mode
        self.calls = 0

    @staticmethod
    def _candidates(messages: list[dict]) -> list[dict]:
        text = messages[0]["content"]
        found = re.search(r"<analysis>\n(.*)\n</analysis>", text, re.DOTALL)
        if not found:
            raise ProviderError("fake provider could not find <analysis> in the prompt")
        return json.loads(found.group(1))["candidates"]

    def complete(self, system: str, messages: list[dict]) -> str:
        self.calls += 1
        candidates = self._candidates(messages)
        picks = [c for c in candidates if c["static_cost_delta"] < 0]

        def reply(chosen):
            return json.dumps({"recommendations": [
                {"candidate_id": c["id"], "reason": "fake model: lowers the static cost"}
                for c in chosen]})

        mode = self.mode
        if mode == "flaky":
            mode = "invalid_json" if self.calls == 1 else "greedy"
        if mode == "greedy":
            return reply(picks)
        if mode == "none":
            return reply([])
        if mode == "invalid_json":
            return "Sure! I would apply the first few candidates."
        if mode == "fenced":
            return "```json\n" + reply(picks) + "\n```"
        if mode == "extra_field":
            data = json.loads(reply(picks))
            data["confidence"] = 0.9
            return json.dumps(data)
        if mode == "unknown_id":
            return json.dumps({"recommendations": [
                {"candidate_id": "C999", "reason": "fake model: invented id"}]})
        # duplicate
        first = (picks or candidates)[:1]
        return reply(first + first)


def make_provider(spec: str) -> Provider:
    """'fake:greedy' -> FakeProvider. Real providers are not available yet."""
    kind, _, option = spec.partition(":")
    if kind == "fake":
        return FakeProvider(option or "greedy")
    raise ProviderError("provider '%s' is not available yet (only 'fake:<mode>' "
                        "exists so far)" % kind)
