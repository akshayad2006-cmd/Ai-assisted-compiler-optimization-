#include "validator/validator.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "ir/tac_parser.h"
#include "optimizer/passes.h"

using json = nlohmann::json;

namespace {

bool instructionHasSideEffect(const Instruction& instruction) {
    if (instruction.opcode == Opcode::INPUT) {
        return true;
    }

    if (instruction.opcode == Opcode::DIV) {
        // Division may fault for a zero divisor or INT_MIN / -1.
        if (!instruction.arg2.isConstant()) {
            return true;
        }

        const int divisor = instruction.arg2.value;
        return divisor == 0 || divisor == -1;
    }

    return false;
}

std::size_t countInputs(const TacProgram& program) {
    return static_cast<std::size_t>(std::count_if(
        program.instructions.begin(),
        program.instructions.end(),
        [](const Instruction& instruction) {
            return instruction.opcode == Opcode::INPUT;
        }
    ));
}

bool hasField(const json& value, const char* field) {
    return value.is_object() && value.contains(field) &&
           !value[field].is_null();
}

std::string readString(
    const json& value,
    const char* field,
    const std::string& fallback = ""
) {
    if (hasField(value, field) && value[field].is_string()) {
        return value[field].get<std::string>();
    }
    return fallback;
}

bool readSize(
    const json& value,
    const char* field,
    std::size_t& output
) {
    if (!hasField(value, field)) {
        return false;
    }

    const json& fieldValue = value[field];

    if (fieldValue.is_number_unsigned()) {
        output = fieldValue.get<std::size_t>();
        return true;
    }

    if (fieldValue.is_number_integer()) {
        const auto number = fieldValue.get<long long>();
        if (number < 0) {
            return false;
        }
        output = static_cast<std::size_t>(number);
        return true;
    }

    return false;
}

void addRejectedDecision(
    ValidationReport& report,
    const RecommendationItem& item,
    const std::string& reason
) {
    report.decisions.push_back({
        item.candidateId,
        "rejected",
        item.rule,
        item.instruction,
        reason
    });
    ++report.rejectedCount;
}

void updateCounts(ValidationReport& report) {
    report.acceptedCount = 0;
    report.rejectedCount = 0;

    for (const auto& decision : report.decisions) {
        if (decision.status == "accepted") {
            ++report.acceptedCount;
        } else if (decision.status == "rejected") {
            ++report.rejectedCount;
        }
    }
}

bool replacementIsAllowed(
    const Candidate& candidate,
    const Instruction& originalInstruction
) {
    // These rules are checked again against the optimizer's actual
    // implementation. Other rules must still match a genuine analyzer
    // candidate, including its exact before/after text.
    if (candidate.rule == "constant_folding") {
        const auto replacement = tryConstantFold(originalInstruction);

        return replacement.has_value() &&
               formatInstruction(*replacement) == candidate.after;
    }

    if (candidate.rule == "algebraic_simplification") {
        const auto replacement = trySimplifyAlgebra(originalInstruction);

        return replacement.has_value() &&
               formatInstruction(*replacement) == candidate.after;
    }

    return true;
}

} // namespace

ValidationReport Validator::validateJson(
    const TacProgram& program,
    const std::string& recommendationsJson
) {
    ValidationReport report;
    report.originalProgram = program;
    report.validatedProgram = program;
    report.status = "invalid_json";
    report.runtimeComparison.equivalent = false;

    json root;

    try {
        root = json::parse(recommendationsJson);
    } catch (const std::exception& error) {
        report.failureMessage =
            std::string("Could not parse recommendation JSON: ") + error.what();
        return report;
    }

    const json* items = nullptr;

    if (root.is_array()) {
        items = &root;
    } else if (root.is_object() &&
               root.contains("recommendations") &&
               root["recommendations"].is_array()) {
        items = &root["recommendations"];
    } else {
        report.failureMessage =
            "Expected a JSON array or an object containing a "
            "'recommendations' array.";
        return report;
    }

    std::vector<RecommendationItem> recommendations;
    recommendations.reserve(items->size());

    for (std::size_t index = 0; index < items->size(); ++index) {
        const json& entry = (*items)[index];

        if (!entry.is_object()) {
            RecommendationItem malformed;
            malformed.candidateId = "INVALID-" + std::to_string(index + 1);
            report.totalRecommendations++;
            addRejectedDecision(
                report, malformed, "Recommendation entry must be a JSON object."
            );
            continue;
        }

        RecommendationItem item;

        item.candidateId = readString(entry, "candidate_id");
        if (item.candidateId.empty()) {
            item.candidateId = readString(entry, "candidateId");
        }

        item.reason = readString(entry, "reason");
        item.rule = readString(entry, "rule");
        item.before = readString(entry, "before");
        item.after = readString(entry, "after");

        if (hasField(entry, "instruction")) {
            if (!readSize(entry, "instruction", item.instruction)) {
                report.totalRecommendations++;
                addRejectedDecision(
                    report,
                    item,
                    "'instruction' must be a non-negative integer."
                );
                continue;
            }
        }

        if (hasField(entry, "test_runtime_check") &&
            entry["test_runtime_check"].is_boolean()) {
            // Compatibility only. This value never disables a check.
            item.testRuntimeCheck =
                entry["test_runtime_check"].get<bool>();
        }

        if (item.candidateId.empty()) {
            report.totalRecommendations++;
            addRejectedDecision(
                report, item, "Missing candidate_id (or candidateId)."
            );
            continue;
        }

        recommendations.push_back(std::move(item));
    }

    ValidationReport validated = validate(program, recommendations);

    // Keep any malformed entries rejected while retaining the decisions
    // generated for all successfully parsed recommendation objects.
    for (const auto& decision : report.decisions) {
        validated.decisions.push_back(decision);
    }

    validated.totalRecommendations = items->size();
    updateCounts(validated);
    return validated;
}

ValidationReport Validator::validate(
    const TacProgram& program,
    const std::vector<RecommendationItem>& recommendations
) {
    ValidationReport report;
    report.status = "ok";
    report.originalProgram = program;
    report.validatedProgram = program;
    report.runtimeComparison.equivalent = true;
    report.totalRecommendations = recommendations.size();

    const AnalysisReport analysis = analyze(program);

    std::unordered_map<std::string, const Candidate*> candidatesById;
    std::unordered_map<std::string, std::size_t> candidatePosition;
    std::unordered_set<std::string> blockedIds;
    std::unordered_map<std::size_t, std::string> blockedByInstruction;

    for (const Candidate& candidate : analysis.candidates) {
        candidatesById[candidate.id] = &candidate;
        candidatePosition[candidate.id] = candidate.instruction;
    }

    for (std::size_t index = 0; index < analysis.blocked.size(); ++index) {
        const std::string id = "B" + std::to_string(index + 1);
        blockedIds.insert(id);
        blockedByInstruction[analysis.blocked[index].instruction] =
            analysis.blocked[index].reason;
    }

    std::unordered_set<std::string> seenIds;
    std::unordered_map<std::size_t, std::size_t> firstByInstruction;
    std::vector<const Candidate*> matched(recommendations.size(), nullptr);
    std::vector<bool> eligible(recommendations.size(), false);
    std::vector<std::string> rejectionReasons(recommendations.size());

    report.decisions.reserve(recommendations.size());

    // First pass: validate candidate identity and metadata.
    for (std::size_t i = 0; i < recommendations.size(); ++i) {
        const RecommendationItem& item = recommendations[i];

        CandidateDecision decision{
            item.candidateId, "rejected", item.rule, item.instruction, ""
        };

        if (item.candidateId.empty()) {
            decision.reason = "Missing candidate ID.";
        } else if (!seenIds.insert(item.candidateId).second) {
            decision.reason = "Duplicate candidate ID.";
        } else if (blockedIds.count(item.candidateId) != 0) {
            decision.reason =
                "This ID refers to a blocked rewrite, not an approved candidate.";
        } else {
            const auto found = candidatesById.find(item.candidateId);

            if (found == candidatesById.end()) {
                decision.reason =
                    "Unknown candidate ID; it was not produced by the analyzer.";
            } else {
                const Candidate& candidate = *found->second;
                matched[i] = &candidate;

                if (candidate.instruction == 0 ||
                    candidate.instruction > program.instructions.size()) {
                    decision.reason =
                        "Candidate refers to an invalid instruction position.";
                } else if (item.instruction != 0 &&
                           item.instruction != candidate.instruction) {
                    decision.reason =
                        "Instruction number does not match the analyzer candidate.";
                } else if (!item.rule.empty() &&
                           item.rule != candidate.rule) {
                    decision.reason =
                        "Rule name does not match the analyzer candidate.";
                } else if (!item.before.empty() &&
                           item.before != candidate.before) {
                    decision.reason =
                        "The 'before' instruction does not match the candidate.";
                } else if (item.after.empty() ||
                           item.after != candidate.after) {
                    // No test_runtime_check exception: arbitrary rewrites
                    // must never bypass the analyzer's approved replacement.
                    decision.reason =
                        "The 'after' instruction must exactly match the "
                        "analyzer-approved candidate.";
                } else {
                    const Instruction& originalInstruction =
                        program.instructions[candidate.instruction - 1];

                    if (candidate.removesInstruction &&
                        instructionHasSideEffect(originalInstruction)) {
                        decision.reason =
                            "Cannot remove an instruction with observable "
                            "side effects or a possible runtime fault.";
                    } else if (!candidate.removesInstruction &&
                               !replacementIsAllowed(
                                   candidate, originalInstruction)) {
                        decision.reason =
                            "The optimizer's rule implementation did not "
                            "confirm this replacement.";
                    } else {
                        eligible[i] = true;
                        decision.status = "pending";
                        decision.reason = "Candidate passed initial checks.";
                    }
                }
            }
        }

        rejectionReasons[i] = decision.reason;
        report.decisions.push_back(std::move(decision));
    }

    // Second pass: resolve multiple recommendations for the same instruction.
    // The first eligible recommendation wins deterministically.
    for (std::size_t i = 0; i < recommendations.size(); ++i) {
        if (!eligible[i] || matched[i] == nullptr) {
            continue;
        }

        const std::size_t instruction = matched[i]->instruction;
        const auto existing = firstByInstruction.find(instruction);

        if (existing != firstByInstruction.end()) {
            eligible[i] = false;
            rejectionReasons[i] =
                "Conflicts with another recommendation for the same instruction.";
            report.decisions[i].reason = rejectionReasons[i];
            continue;
        }

        firstByInstruction[instruction] = i;
    }

    // Third pass: repeatedly reject candidates whose dependencies are not
    // also eligible. Repeat because rejecting one candidate can invalidate
    // candidates that depend on it.
    bool changed = true;

    while (changed) {
        changed = false;

        for (std::size_t i = 0; i < recommendations.size(); ++i) {
            if (!eligible[i] || matched[i] == nullptr) {
                continue;
            }

            for (const std::string& dependency : matched[i]->requires_) {
                bool dependencySelected = false;

                for (std::size_t j = 0; j < recommendations.size(); ++j) {
                    if (eligible[j] &&
                        recommendations[j].candidateId == dependency) {
                        dependencySelected = true;
                        break;
                    }
                }

                if (!dependencySelected) {
                    eligible[i] = false;
                    rejectionReasons[i] =
                        "Required candidate '" + dependency +
                        "' is missing, invalid, or rejected.";
                    report.decisions[i].reason = rejectionReasons[i];
                    changed = true;
                    break;
                }
            }
        }
    }

    // Apply only the exact replacements approved by the analyzer.
    // Replacements/removals are collected against original instruction
    // positions, so earlier removals cannot shift later positions.
    std::unordered_map<std::size_t, const Candidate*> acceptedByInstruction;

    for (std::size_t i = 0; i < recommendations.size(); ++i) {
        if (!eligible[i] || matched[i] == nullptr) {
            continue;
        }

        acceptedByInstruction[matched[i]->instruction] = matched[i];
        report.decisions[i].status = "accepted";
        report.decisions[i].reason =
            "Candidate matched the analyzer and passed validation checks.";
    }

    TacProgram validated;
    validated.instructions.reserve(program.instructions.size());

    for (std::size_t index = 0; index < program.instructions.size(); ++index) {
        const std::size_t instructionNumber = index + 1;
        const auto found = acceptedByInstruction.find(instructionNumber);

        if (found == acceptedByInstruction.end()) {
            validated.instructions.push_back(program.instructions[index]);
            continue;
        }

        const Candidate& candidate = *found->second;

        if (candidate.removesInstruction) {
            continue;
        }

        const TacParseResult parsed = parseTac(candidate.after);

        if (!parsed.ok() || parsed.program.instructions.size() != 1) {
            // Defensive safeguard: an analyzer candidate that cannot be
            // parsed must not be applied.
            for (std::size_t i = 0; i < recommendations.size(); ++i) {
                if (eligible[i] && matched[i] == &candidate) {
                    eligible[i] = false;
                    report.decisions[i].status = "rejected";
                    report.decisions[i].reason =
                        "The approved replacement could not be parsed as "
                        "exactly one TAC instruction.";
                    break;
                }
            }

            validated.instructions.push_back(program.instructions[index]);
            continue;
        }

        validated.instructions.push_back(parsed.program.instructions.front());
    }

    // Run differential runtime checks on the proposed program.
    const std::vector<std::vector<int>> testVectors =
        TacInterpreter::defaultTestVectors(countInputs(program));

    report.runtimeComparison =
        TacInterpreter::compare(program, validated, testVectors);

    if (!report.runtimeComparison.equivalent) {
        report.failureMessage =
            "Differential runtime verification failed: " +
            report.runtimeComparison.mismatchReason;

        // Fail closed: do not return an optimized program that failed
        // differential testing.
        report.validatedProgram = program;

        for (auto& decision : report.decisions) {
            if (decision.status == "accepted") {
                decision.status = "rejected";
                decision.reason =
                    "Rejected because differential runtime verification failed.";
            }
        }

        report.status = "runtime_check_failed";
    } else {
        report.validatedProgram = std::move(validated);
        report.status = "ok";
    }

    for (std::size_t i = 0; i < recommendations.size(); ++i) {
        if (report.decisions[i].status == "pending") {
            report.decisions[i].status = "rejected";
            report.decisions[i].reason =
                rejectionReasons[i].empty()
                    ? "Candidate did not pass validation."
                    : rejectionReasons[i];
        }
    }

    updateCounts(report);
    return report;
}

std::string Validator::formatReportJson(const ValidationReport& report) {
    json output;

    output["status"] = report.status;
    output["summary"] = {
        {"total_recommendations", report.totalRecommendations},
        {"accepted", report.acceptedCount},
        {"rejected", report.rejectedCount}
    };

    output["decisions"] = json::array();

    for (const CandidateDecision& decision : report.decisions) {
        output["decisions"].push_back({
            {"candidate_id", decision.candidateId},
            {"status", decision.status},
            {"rule", decision.rule},
            {"instruction", decision.instruction},
            {"reason", decision.reason}
        });
    }

    output["runtime_check"] = {
        {"equivalent", report.runtimeComparison.equivalent},
        {"tests_run", report.runtimeComparison.testsRun},
        {"mismatch_reason", report.runtimeComparison.mismatchReason},
        {"limitation",
         "Sampled runtime equivalence is not a formal proof of universal "
         "semantic equivalence."}
    };

    output["failure_message"] = report.failureMessage;
    output["validated_tac"] = formatTac(report.validatedProgram);

    return output.dump(2) + "\n";
}
