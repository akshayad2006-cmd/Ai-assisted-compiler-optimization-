"""The recommender: analysis in, validated recommendations out.

Flow: build prompt -> ask the provider -> check the reply strictly -> if the
reply is badly formed, tell the model what was wrong and ask again (a few
times) -> otherwise give up. A failed run NEVER produces made-up
recommendations: status "failed" means "no recommendation".
"""
from __future__ import annotations

from recommender.prompt import SYSTEM_PROMPT, build_retry_message, build_user_prompt
from recommender.providers import Provider, ProviderError
from recommender.schema import SchemaError, parse_model_reply, validate_recommendations

DEFAULT_MAX_ATTEMPTS = 3


class AnalysisError(ValueError):
    """The analysis JSON is not what the compiler's --analyze prints."""


def check_analysis(analysis) -> None:
    """Minimal shape check of the compiler's analysis JSON."""
    if not isinstance(analysis, dict):
        raise AnalysisError("the analysis must be a JSON object")
    if analysis.get("schema_version") != 1:
        raise AnalysisError("unsupported analysis schema_version: %r"
                            % (analysis.get("schema_version"),))
    for key in ("program", "instructions", "candidates", "blocked"):
        if key not in analysis:
            raise AnalysisError("the analysis has no '%s'" % key)
    if not isinstance(analysis["candidates"], list):
        raise AnalysisError("'candidates' must be a list")


def recommend(analysis: dict, provider: Provider,
              max_attempts: int = DEFAULT_MAX_ATTEMPTS) -> dict:
    """Asks 'provider' which candidates to apply. Returns a result dict:

    status           "ok" or "failed"
    provider         provider name, e.g. "fake:greedy"
    is_real_ai       False for the fake provider
    attempts         how many times the model was asked
    recommendations  list of {"candidate_id", "reason"} (empty if failed)
    errors           why it failed, or what was wrong with rejected replies
    """
    check_analysis(analysis)
    result = {"status": "ok", "provider": provider.name,
              "is_real_ai": provider.is_real_ai, "attempts": 0,
              "recommendations": [], "errors": []}

    if not analysis["candidates"]:
        return result  # nothing to choose from: do not spend a model call

    valid_ids = {candidate["id"] for candidate in analysis["candidates"]}
    messages = [{"role": "user", "content": build_user_prompt(analysis)}]

    for attempt in range(1, max_attempts + 1):
        result["attempts"] = attempt
        try:
            reply = provider.complete(SYSTEM_PROMPT, messages)
        except ProviderError as error:
            result["status"] = "failed"
            result["errors"].append("provider error: %s" % error)
            return result
        try:
            chosen = validate_recommendations(parse_model_reply(reply), valid_ids)
        except SchemaError as error:
            result["errors"].extend("attempt %d: %s" % (attempt, p) for p in error.problems)
            messages.append({"role": "assistant", "content": reply})
            messages.append({"role": "user", "content": build_retry_message(error.problems)})
            continue
        result["recommendations"] = [{"candidate_id": r.candidate_id, "reason": r.reason}
                                     for r in chosen]
        return result

    result["status"] = "failed"
    result["errors"].append("no well-formed reply after %d attempt(s)" % max_attempts)
    return result
