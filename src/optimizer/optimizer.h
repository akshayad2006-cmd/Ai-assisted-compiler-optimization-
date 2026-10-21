// optimizer.h - the rule-based optimization engine.
//
// An OPTIMIZATION PASS is one rule applied over the whole program.
// The optimizer runs all passes repeatedly until a full round changes
// nothing, because one optimization can unlock another.
#pragma once

#include <string>
#include <vector>

#include "ir/tac.h"
#include "optimizer/change.h"

struct OptimizeResult {
    TacProgram program;            // the optimized program
    std::vector<Change> changes;   // every rewrite, in the order applied
};

class Optimizer {
public:
    // Never modifies 'input'; returns an optimized copy plus a change log.
    static OptimizeResult optimize(const TacProgram& input);
};

// Change log as text, one line per change plus a "Total changes: N" line.
std::string formatChanges(const OptimizeResult& result);
