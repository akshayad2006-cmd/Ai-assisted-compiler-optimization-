#include "performance/performance_analyzer.h"

#include <iomanip>
#include <sstream>

#include "analysis/analyzer.h"
#include "ir/tac.h"

std::string formatPerformanceReport(
    const TacProgram& original,
    const OptimizeResult& optimized) {
    const AnalysisReport analysis = analyze(original);

    const std::size_t originalCount = original.instructions.size();
    const std::size_t optimizedCount = optimized.program.instructions.size();

    const double instructionReduction =
        originalCount == 0
            ? 0.0
            : 100.0 * static_cast<double>(originalCount - optimizedCount) /
                  static_cast<double>(originalCount);

    const int originalCost = analysis.staticCost;
    const int optimizedCost = analysis.optimizedStaticCost;

    const double costReduction =
        originalCost <= 0
            ? 0.0
            : 100.0 * static_cast<double>(originalCost - optimizedCost) /
                  static_cast<double>(originalCost);

    std::ostringstream out;
    out << "PERFORMANCE ANALYSIS REPORT\n";
    out << "===========================\n\n";

    out << "1. TAC METRICS\n";
    out << "Original instruction count : " << originalCount << '\n';
    out << "Optimized instruction count: " << optimizedCount << '\n';
    out << "Instructions removed      : "
        << (originalCount >= optimizedCount
                ? originalCount - optimizedCount
                : 0)
        << '\n';

    out << std::fixed << std::setprecision(2);
    out << "Instruction reduction     : " << instructionReduction << "%\n\n";

    out << "2. ESTIMATED STATIC COST\n";
    out << "Original estimated cost   : " << originalCost << '\n';
    out << "Optimized estimated cost  : " << optimizedCost << '\n';
    out << "Estimated cost reduction  : " << costReduction << "%\n";
    out << "Cost model: copy/add/sub=1, mul=3, div=10, input=5, print=2.\n";
    out << "These costs are estimates, not measured execution time.\n\n";

    out << "3. OPTIMIZATION SUMMARY\n";
    out << "Optimization changes applied: "
        << optimized.changes.size() << '\n';

    out << "\n4. LLVM STATUS\n";
    out << "LLVM IR generation: performed separately by --llvm.\n";
    out << "LLVM O2 verification and native assembly generation are not "
           "measured by this report.\n\n";

    out << "5. INTERPRETATION\n";
    if (originalCount > optimizedCount || originalCost > optimizedCost) {
        out << "The rule-based optimizer reduced the static program metrics.\n";
    } else {
        out << "No reduction was recorded by the static metrics for this program.\n";
    }
    out << "Runtime speedup has not been measured by this report.\n";

    return out.str();
}
