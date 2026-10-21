#pragma once

#include <string>

#include "ir/tac.h"
#include "optimizer/optimizer.h"

// Builds a performance report from the original TAC and optimization result.
// LLVM tool execution is handled by the CLI integration.
std::string formatPerformanceReport(
    const TacProgram& original,
    const OptimizeResult& optimized);
