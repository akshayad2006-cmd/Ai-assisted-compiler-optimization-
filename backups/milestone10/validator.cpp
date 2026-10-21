#include "validator/validator.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "common/diagnostic.h"
#include "ir/tac_parser.h"
#include "optimizer/passes.h"

using json = nlohmann::json;

namespace {

// Safety check: returns true if executing this instruction performs an observable
// side effect or can fault at runtime, and therefore must never be eliminated.
bool instructionHasSideEffect(const Instruction& instruction) {
    if (instruction.opcode == Opcode::INPUT) {
        return true;
    }
    if (instruction.opcode == Opcode::DIV) {
        // Safe only when the divisor is a constant other than 0 (division by zero)
        // and other than -1 (INT_MIN / -1 overflows). Anything else could fault at runtime.
        return !(instruction.arg2.isConstant() &&
                 instruction.arg2.value != 0 &&
                 instruction.arg2.value != -1);
    }
    return false;
}

// Counts the number of input() instructions in a TAC program.
std::size_t countInputs(const TacProgram& program) {
    std::size_t count = 0;
    for (const Instruction& inst : program.instructions) {
        if (inst.opcode == Opcode::INPUT) {
            count++;
        }
    }
    return count;
}

}  // namespace

ValidationReport Validator::validateJson(
    const TacProgram& program,
    const std::string& recommendationsJson) {

    ValidationReport report;
    report.originalProgram = program;
    report.validatedProgram = program;

    json root;
    try {
        root = json::parse(recommendationsJson);
    } catch (const json::exception& e) {
        report.status = "rejected";
        report.failureMessage = std::string("malformed recommendations JSON: ") + e.what();
        return report;
    }

    std::vector<RecommendationItem> items;

    // Handle either an array directly, or an object with "recommendations"
    json recArray;
    if (root.is_array()) {
        recArray = root;
    } else if (root.is_object()) {
        if (!root.contains("recommendations") || !root["recommendations"].is_array()) {
            report.status = "rejected";
            report.failureMessage = "JSON object must contain a 'recommendations' array";
            return report;
        }
        recArray = root["recommendations"];
    } else {
        report.status = "rejected";
        report.failureMessage = "top-level recommendations JSON must be an object or array";
        return report;
    }

    std::unordered_set<std::string> seenIds;
    for (const auto& elem : recArray) {
        if (!elem.is_object()) {
            report.status = "rejected";
            report.failureMessage = "each recommendation must be a JSON object";
            return report;
        }

        std::string cid;
        if (elem.contains("candidate_id") && elem["candidate_id"].is_string()) {
            cid = elem["candidate_id"].get<std::string>();
        } else if (elem.contains("candidateId") && elem["candidateId"].is_string()) {
            cid = elem["candidateId"].get<std::string>();
        } else {
            report.status = "rejected";
            report.failureMessage = "recommendation item is missing a string 'candidate_id'";
            return report;
        }

        RecommendationItem item;
        item.candidateId = cid;
        if (elem.contains("reason") && elem["reason"].is_string()) {
            item.reason = elem["reason"].get<std::string>();
        }
        if (elem.contains("rule") && elem["rule"].is_string()) {
            item.rule = elem["rule"].get<std::string>();
        }
        if (elem.contains("instruction") && elem["instruction"].is_number_unsigned()) {
            item.instruction = elem["instruction"].get<std::size_t>();
        }
        if (elem.contains("before") && elem["before"].is_string()) {
            item.before = elem["before"].get<std::string>();
        }
        if (elem.contains("after") && elem["after"].is_string()) {
            item.after = elem["after"].get<std::string>();
        }
        if (elem.contains("test_runtime_check") && elem["test_runtime_check"].is_boolean()) {
            item.testRuntimeCheck = elem["test_runtime_check"].get<bool>();
        }

        items.push_back(item);
    }

    return validate(program, items);
}

ValidationReport Validator::validate(
    const TacProgram& program,
    const std::vector<RecommendationItem>& recommendations) {

    ValidationReport report;
    report.originalProgram = program;
    report.validatedProgram = program;
    report.totalRecommendations = recommendations.size();

    // 1. Run actual analysis on unchanged program to get ground truth candidates and blocked rewrites.
    const AnalysisReport analysis = analyze(program);

    std::unordered_map<std::string, Candidate> candidateById;
    for (const Candidate& c : analysis.candidates) {
        candidateById[c.id] = c;
    }

    std::unordered_map<std::size_t, BlockedRewrite> blockedByInstruction;
    for (const BlockedRewrite& b : analysis.blocked) {
        blockedByInstruction[b.instruction] = b;
    }

    std::unordered_map<std::string, BlockedRewrite> blockedById;
    for (std::size_t bIdx = 0; bIdx < analysis.blocked.size(); ++bIdx) {
        blockedById["B" + std::to_string(bIdx + 1)] = analysis.blocked[bIdx];
    }

    std::vector<CandidateDecision> decisions(recommendations.size());
    std::unordered_set<std::string> seenCandidateIds;
    std::vector<std::size_t> validItemIndices;
    std::unordered_map<std::string, std::string> customReplacementByCandidateId;

    // 2. Initial pass: inspect each recommendation against ground truth analysis report.
    for (std::size_t i = 0; i < recommendations.size(); ++i) {
        const RecommendationItem& item = recommendations[i];
        CandidateDecision& dec = decisions[i];
        dec.candidateId = item.candidateId;

        // Detect duplicate candidate IDs in recommendations
        if (seenCandidateIds.count(item.candidateId) != 0) {
            dec.status = "rejected";
            dec.reason = "duplicate candidate ID '" + item.candidateId + "' in recommendations";
            continue;
        }
        seenCandidateIds.insert(item.candidateId);

        // Check 1: Blocked candidate ID (e.g. B1, B2) or unknown candidate ID
        const auto blockIdIt = blockedById.find(item.candidateId);
        if (blockIdIt != blockedById.end()) {
            dec.status = "rejected";
            dec.rule = blockIdIt->second.rule;
            dec.instruction = blockIdIt->second.instruction;
            dec.reason = "blocked candidate on instruction " +
                         std::to_string(blockIdIt->second.instruction) +
                         ": " + blockIdIt->second.reason;
            continue;
        }

        const auto candIt = candidateById.find(item.candidateId);
        if (candIt == candidateById.end()) {
            if (item.instruction != 0 && blockedByInstruction.count(item.instruction) != 0) {
                const auto blockInstIt = blockedByInstruction.find(item.instruction);
                dec.status = "rejected";
                dec.rule = blockInstIt->second.rule;
                dec.instruction = item.instruction;
                dec.reason = "blocked candidate on instruction " +
                             std::to_string(item.instruction) +
                             ": " + blockInstIt->second.reason;
                continue;
            }
            dec.status = "rejected";
            dec.reason = "unknown candidate ID '" + item.candidateId + "' (not found in analysis report)";
            continue;
        }

        const Candidate& cand = candIt->second;
        dec.rule = cand.rule;
        dec.instruction = cand.instruction;

        // Check 2: Blocked rewrite targeting this instruction
        const auto blockIt = blockedByInstruction.find(cand.instruction);
        if (blockIt != blockedByInstruction.end()) {
            dec.status = "rejected";
            dec.reason = "blocked candidate on instruction " + std::to_string(cand.instruction) +
                         ": " + blockIt->second.reason;
            continue;
        }

        // Check 3: Claimed fields verification (never blindly trust external recommendation metadata)
        if (!item.rule.empty() && item.rule != cand.rule) {
            dec.status = "rejected";
            dec.reason = "rule mismatch: recommendation claimed '" + item.rule +
                         "' but analysis candidate rule is '" + cand.rule + "'";
            continue;
        }
        if (item.instruction != 0 && item.instruction != cand.instruction) {
            dec.status = "rejected";
            dec.reason = "instruction index mismatch: recommendation claimed " +
                         std::to_string(item.instruction) +
                         " but analysis candidate is " +
                         std::to_string(cand.instruction);
            continue;
        }
        if (!item.before.empty() && item.before != cand.before) {
            dec.status = "rejected";
            dec.reason = "instruction text mismatch: recommendation before text does not match program instruction";
            continue;
        }
        if (!item.after.empty() && item.after != cand.after) {
            if (!item.testRuntimeCheck) {
                dec.status = "rejected";
                dec.reason = "proposed transformation text does not match verified compiler rule output";
                continue;
            }
            customReplacementByCandidateId[cand.id] = item.after;
        }

        // Check 4: Preserve observable input operations and potentially faulting operations
        if (cand.instruction == 0 || cand.instruction > program.instructions.size()) {
            dec.status = "rejected";
            dec.reason = "invalid instruction index out of program bounds";
            continue;
        }
        const Instruction& targetInst = program.instructions[cand.instruction - 1];
        if (cand.removesInstruction && instructionHasSideEffect(targetInst)) {
            dec.status = "rejected";
            dec.reason = "cannot eliminate instruction with observable side effect or potential fault";
            continue;
        }

        // Check 5: Verify transformation against actual compiler transformation logic.
        if (!item.testRuntimeCheck) {
            if (cand.rule == "constant_folding") {
                const auto folded = tryConstantFold(targetInst);
                if (!folded || formatInstruction(*folded) != cand.after) {
                    dec.status = "rejected";
                    dec.reason =
                        "compiler constant folding verification failed on instruction";
                    continue;
                }
            } else if (cand.rule == "algebraic_simplification") {
                const auto simplified = trySimplifyAlgebra(targetInst);
                if (!simplified || formatInstruction(*simplified) != cand.after) {
                    dec.status = "rejected";
                    dec.reason =
                        "compiler algebraic simplification verification failed on instruction";
                    continue;
                }
            }
        }

        // Passed initial checks; eligible for conflict resolution and dependency validation.
        validItemIndices.push_back(i);
    }

    // 3. Conflict resolution: detect conflicting candidates targeting the same instruction.
    // The first candidate claims the instruction; subsequent conflicting candidates are rejected.
    std::unordered_map<std::size_t, std::string> instructionClaimedBy;
    std::vector<std::size_t> nonConflictingIndices;

    for (std::size_t idx : validItemIndices) {
        const std::string& cid = recommendations[idx].candidateId;
        const Candidate& cand = candidateById[cid];
        const std::size_t targetInst = cand.instruction;

        const auto claimIt = instructionClaimedBy.find(targetInst);
        if (claimIt != instructionClaimedBy.end()) {
            decisions[idx].status = "rejected";
            decisions[idx].reason = "conflicts with candidate '" + claimIt->second +
                                   "' targeting the same instruction (" + std::to_string(targetInst) + ")";
        } else {
            instructionClaimedBy[targetInst] = cid;
            nonConflictingIndices.push_back(idx);
        }
    }

    // 4. Multi-pass dependency validation: RUNS AFTER conflict resolution.
    // Candidates whose required partners are missing from the recommendation or were rejected
    // are rejected deterministically until a stable fixed-point is reached.
    std::set<std::string> activeCandidateIds;
    std::unordered_map<std::string, std::size_t> idToRecIndex;
    for (std::size_t idx : nonConflictingIndices) {
        const std::string& cid = recommendations[idx].candidateId;
        activeCandidateIds.insert(cid);
        idToRecIndex[cid] = idx;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto it = activeCandidateIds.begin(); it != activeCandidateIds.end();) {
            const std::string& cid = *it;
            const Candidate& cand = candidateById[cid];
            bool dependenciesMet = true;
            std::string missingDep;

            for (const std::string& reqId : cand.requires_) {
                if (activeCandidateIds.count(reqId) == 0) {
                    dependenciesMet = false;
                    missingDep = reqId;
                    break;
                }
            }

            if (!dependenciesMet) {
                const std::size_t recIdx = idToRecIndex[cid];
                decisions[recIdx].status = "rejected";
                decisions[recIdx].reason = "missing required dependency '" + missingDep +
                                          "' (dependency was not recommended, conflicted, or rejected)";
                it = activeCandidateIds.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
    }

    // 5. Finalize decisions for accepted candidates.
    for (const std::string& acceptedId : activeCandidateIds) {
        const std::size_t recIdx = idToRecIndex[acceptedId];
        const Candidate& cand = candidateById[acceptedId];
        decisions[recIdx].status = "accepted";
        decisions[recIdx].reason = "accepted: verified " + cand.rule + " on instruction " +
                                  std::to_string(cand.instruction);
    }

    // 6. Apply only accepted, valid transformations through real compiler passes.
    TacProgram validated = program;
    std::vector<bool> keep(validated.instructions.size(), true);

    // Collect accepted candidates sorted by instruction index
    std::vector<Candidate> acceptedCandidates;
    for (const std::string& acceptedId : activeCandidateIds) {
        acceptedCandidates.push_back(candidateById[acceptedId]);
    }
    std::sort(acceptedCandidates.begin(), acceptedCandidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.instruction < b.instruction; });

    for (const Candidate& cand : acceptedCandidates) {
        const std::size_t instIdx = cand.instruction - 1;
        if (cand.removesInstruction) {
            keep[instIdx] = false;
        } else {
            const auto customIt = customReplacementByCandidateId.find(cand.id);
            const std::string textAfter = (customIt != customReplacementByCandidateId.end())
                                              ? customIt->second
                                              : cand.after;
            const TacParseResult parsed = parseTac(textAfter + "\n");
            if (parsed.ok() && parsed.program.instructions.size() == 1) {
                validated.instructions[instIdx] = parsed.program.instructions[0];
            }
        }
    }

    std::vector<Instruction> remainingInstructions;
    for (std::size_t k = 0; k < validated.instructions.size(); ++k) {
        if (keep[k]) {
            remainingInstructions.push_back(validated.instructions[k]);
        }
    }
    validated.instructions = std::move(remainingInstructions);

    // 7. Differential Runtime Verification:
    // Execute both original and validated program on test vectors to compare observable outputs.
    const std::size_t inputCount = countInputs(program);
    const auto testVectors = TacInterpreter::defaultTestVectors(inputCount);
    const DifferentialComparison comparison = TacInterpreter::compare(program, validated, testVectors);

    report.runtimeComparison = comparison;
    report.decisions = decisions;

    if (!comparison.equivalent) {
        // Runtime output mismatch detected! Reject transformation for safety.
        report.status = "rejected";
        report.failureMessage = "runtime verification detected behavioral mismatch: " +
                               comparison.mismatchReason;
        report.validatedProgram = program;  // revert to original safe program
        report.acceptedCount = 0;
        report.rejectedCount = recommendations.size();
        for (CandidateDecision& dec : report.decisions) {
            if (dec.status == "accepted") {
                dec.status = "rejected";
                dec.reason = "reverted: program transformation failed runtime differential verification";
            }
        }
    } else {
        report.status = "ok";
        report.validatedProgram = validated;
        report.acceptedCount = activeCandidateIds.size();
        report.rejectedCount = recommendations.size() - activeCandidateIds.size();
    }

    return report;
}

std::string Validator::formatReportJson(const ValidationReport& report) {
    json root;
    root["status"] = report.status;
    if (!report.failureMessage.empty()) {
        root["failure_message"] = report.failureMessage;
    }

    json summary;
    summary["total_recommendations"] = report.totalRecommendations;
    summary["accepted"] = report.acceptedCount;
    summary["rejected"] = report.rejectedCount;
    root["summary"] = summary;

    json decisionsArray = json::array();
    for (const CandidateDecision& dec : report.decisions) {
        json decObj;
        decObj["candidate_id"] = dec.candidateId;
        decObj["status"] = dec.status;
        if (!dec.rule.empty()) decObj["rule"] = dec.rule;
        if (dec.instruction > 0) decObj["instruction"] = dec.instruction;
        decObj["reason"] = dec.reason;
        decisionsArray.push_back(decObj);
    }
    root["decisions"] = decisionsArray;

    json runtimeObj;
    runtimeObj["status"] = report.runtimeComparison.equivalent ? "passed" : "mismatch";
    runtimeObj["tests_run"] = report.runtimeComparison.testsRun;
    if (!report.runtimeComparison.mismatchReason.empty()) {
        runtimeObj["mismatch_reason"] = report.runtimeComparison.mismatchReason;
    }
    runtimeObj["disclaimer"] =
        "Sampled runtime equivalence is empirical testing evidence over selected inputs, "
        "not formal mathematical proof of universal semantic equivalence.";
    root["runtime_check"] = runtimeObj;

    std::vector<std::string> tacLines;
    std::istringstream tacStream(formatTac(report.validatedProgram));
    std::string line;
    while (std::getline(tacStream, line)) {
        if (!line.empty()) {
            tacLines.push_back(line);
        }
    }
    root["validated_tac"] = tacLines;

    return root.dump(2) + "\n";
}
