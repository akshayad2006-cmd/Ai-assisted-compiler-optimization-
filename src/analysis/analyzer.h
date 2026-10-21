// analyzer.h - optimization ANALYSIS: "what could be optimized here?"
//
// The optimizer (milestone 7) just rewrites the program. The analyzer only
// LOOKS. It asks every optimization rule "where would you apply?" on the
// program exactly as given and reports each answer as a CANDIDATE.
//
// Candidates are plain structured data (printed as JSON). Later milestones
// hand this list to an AI model, which may only pick from it, and to a
// validator, which double-checks the pick.
//
// The analyzer reuses the optimizer's own rules instead of re-implementing
// them, so "candidate" and "what the optimizer really does" cannot disagree.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ir/tac.h"

// One place where an optimization rule could be applied.
struct Candidate {
    std::string id;          // "C1", "C2", ... in program order
    std::string rule;        // e.g. "constant_folding"
    std::size_t instruction = 0;  // 1-based position of the instruction
    std::string before;      // the instruction as it is now
    std::string after;       // what it would become, or "(removed)"
    bool removesInstruction = false;
    int costDelta = 0;       // static cost estimate: cost(after) - cost(before)
    // Other candidates that must be applied together with this one.
    // Only dead-code removals have these: if "t1 = a * b" is dead only
    // because "t2 = t1 + 1" is dead too, both must go.
    std::vector<std::string> requires_;
};

// A rewrite that LOOKS possible but is NOT safe, with the reason.
// Showing these lets later stages catch unsafe AI suggestions.
struct BlockedRewrite {
    std::string rule;
    std::size_t instruction = 0;  // 1-based
    std::string text;
    std::string reason;
};

struct AnalysisReport {
    TacProgram program;                   // the program that was analyzed
    std::vector<Candidate> candidates;
    std::vector<BlockedRewrite> blocked;
    int staticCost = 0;                   // estimated cost of 'program'
    std::size_t optimizedInstructionCount = 0;  // after the full optimizer
    int optimizedStaticCost = 0;
};

// Static cost of one instruction: a made-up but reasonable model
// (copy/add/sub 1, mul 3, div 10, input 5, print 2). It is an ESTIMATE
// from a table, never a measurement of real running time.
int instructionCost(const Instruction& instruction);

// Analyzes 'program' (never modifies it).
AnalysisReport analyze(const TacProgram& program);

// The report as JSON text (ends with a newline).
std::string formatAnalysisJson(const AnalysisReport& report);
