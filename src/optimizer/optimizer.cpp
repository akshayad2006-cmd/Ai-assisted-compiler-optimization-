#include "optimizer/optimizer.h"

#include <functional>
#include <optional>

#include "optimizer/passes.h"

namespace {

using InstructionRule = std::optional<Instruction> (*)(const Instruction&);
using Pass = std::function<std::vector<Change>(TacProgram&)>;

// Turns an instruction rule into a whole-program pass: apply the rule to
// every instruction and record each rewrite.
std::vector<Change> applyRule(const char* name, InstructionRule rule, TacProgram& program) {
    std::vector<Change> changes;
    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        const std::optional<Instruction> replacement = rule(program.instructions[i]);
        if (!replacement) {
            continue;
        }
        changes.push_back(Change{name, i, formatInstruction(program.instructions[i]),
                                 formatInstruction(*replacement)});
        program.instructions[i] = *replacement;
    }
    return changes;
}

// Safety limit. Every rewrite makes the program strictly simpler (fewer
// non-constant operands, fewer operations or fewer instructions), so the
// loop always ends long before this; the limit only guards against bugs.
constexpr int kMaxRounds = 100;

}  // namespace

OptimizeResult Optimizer::optimize(const TacProgram& input) {
    OptimizeResult result;
    result.program = input;

    // Passes run in this order within every round.
    const std::vector<Pass> passes = {
        propagateConstants,
        [](TacProgram& p) { return applyRule("constant_folding", tryConstantFold, p); },
        [](TacProgram& p) { return applyRule("algebraic_simplification", trySimplifyAlgebra, p); },
        propagateCopies,
        eliminateCommonSubexpressions,
        eliminateDeadCode,
    };

    for (int round = 0; round < kMaxRounds; ++round) {
        bool changedThisRound = false;

        for (const Pass& pass : passes) {
            const std::vector<Change> changes = pass(result.program);
            if (!changes.empty()) {
                changedThisRound = true;
                result.changes.insert(result.changes.end(), changes.begin(), changes.end());
            }
        }

        if (!changedThisRound) {
            break;  // fixed point reached: nothing more to optimize
        }
    }
    return result;
}

std::string formatChanges(const OptimizeResult& result) {
    std::string text;
    for (const Change& change : result.changes) {
        text += "[" + change.pass + "] " + change.before + "  ->  " + change.after + "\n";
    }
    text += "Total changes: " + std::to_string(result.changes.size()) + "\n";
    return text;
}
