#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "analysis/analyzer.h"
#include "ir/tac.h"
#include "ir/tac_interpreter.h"

struct RecommendationItem {
    std::string candidateId;
    std::string reason;
    std::string rule;
    std::size_t instruction = 0;
    std::string before;
    std::string after;

    // Retained for compatibility with existing recommendation JSON.
    // This flag must never bypass validation.
    bool testRuntimeCheck = false;
};

struct CandidateDecision {
    std::string candidateId;
    std::string status;
    std::string rule;
    std::size_t instruction = 0;
    std::string reason;
};

struct ValidationReport {
    std::string status;
    std::size_t totalRecommendations = 0;
    std::size_t acceptedCount = 0;
    std::size_t rejectedCount = 0;
    std::vector<CandidateDecision> decisions;
    TacProgram originalProgram;
    TacProgram validatedProgram;
    DifferentialComparison runtimeComparison;
    std::string failureMessage;
};

class Validator {
public:
    static ValidationReport validateJson(
        const TacProgram& program,
        const std::string& recommendationsJson
    );

    static ValidationReport validate(
        const TacProgram& program,
        const std::vector<RecommendationItem>& recommendations
    );

    static std::string formatReportJson(const ValidationReport& report);
};
