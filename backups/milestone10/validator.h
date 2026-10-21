// validator.h - Deterministic C++ validator for AI optimization recommendations.
//
// Milestone 10:
// Validates recommendations from an AI recommender against compiler analysis,
// resolves conflicts deterministically, enforces dependency constraints,
// checks preservation of side effects and observable semantics, and verifies
// runtime equivalence via differential simulation.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "analysis/analyzer.h"
#include "ir/tac.h"
#include "ir/tac_interpreter.h"

// Input recommendation as parsed from JSON.
struct RecommendationItem {
    std::string candidateId;
    std::string reason;

    // Optional claimed fields for strict validation against actual candidates.
    std::string rule;
    std::size_t instruction = 0;
    std::string before;
    std::string after;
    bool testRuntimeCheck = false;  // used specifically to test differential verification on mismatched output
};

// Result decision for one recommendation candidate.
struct CandidateDecision {
    std::string candidateId;
    std::string status;  // "accepted" or "rejected"
    std::string rule;
    std::size_t instruction = 0;
    std::string reason;
};

// Complete validation report produced by the validator.
struct ValidationReport {
    std::string status;  // "ok" (if validation succeeded) or "rejected"
    std::size_t totalRecommendations = 0;
    std::size_t acceptedCount = 0;
    std::size_t rejectedCount = 0;
    std::vector<CandidateDecision> decisions;
    TacProgram originalProgram;
    TacProgram validatedProgram;
    DifferentialComparison runtimeComparison;
    std::string failureMessage;  // if top-level validation failed (e.g. malformed JSON)
};

class Validator {
public:
    // Validates recommendations JSON string against 'program'.
    static ValidationReport validateJson(
        const TacProgram& program,
        const std::string& recommendationsJson);

    // Validates a structured list of recommendations against 'program'.
    static ValidationReport validate(
        const TacProgram& program,
        const std::vector<RecommendationItem>& recommendations);

    // Serializes a ValidationReport into formatted JSON.
    static std::string formatReportJson(const ValidationReport& report);
};
