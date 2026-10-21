#include "optimizer/passes.h"

#include <climits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

// Source variables and compiler temporaries live in separate namespaces:
// a user variable named "t1" is NOT the temporary t1. The key adds the kind.
std::string operandKey(const Operand& operand) {
    return (operand.isTemporary() ? "t:" : "v:") + operand.name;
}

bool isNamed(const Operand& operand) {
    return operand.isVariable() || operand.isTemporary();
}

// True if executing this instruction does something observable besides
// producing its result, so it must never be removed even when the result is
// unused:
//  * input() consumes a value from the program's input;
//  * a division might fail at runtime (divide by zero, or INT_MIN / -1 which
//    overflows) unless the divisor is a constant other than 0 and -1.
bool hasSideEffect(const Instruction& instruction) {
    if (instruction.opcode == Opcode::INPUT) {
        return true;
    }
    if (instruction.opcode == Opcode::DIV) {
        // Safe only when the divisor is a constant other than 0 (division by
        // zero) and -1 (INT_MIN / -1 overflows). Anything else might fail.
        return !(instruction.arg2.isConstant() && instruction.arg2.value != 0 &&
                 instruction.arg2.value != -1);
    }
    return false;
}

}  // namespace

std::optional<Instruction> tryConstantFold(const Instruction& instruction) {
    if (!instruction.isBinary() || !instruction.arg1.isConstant() ||
        !instruction.arg2.isConstant()) {
        return std::nullopt;
    }

    // Compute in 64 bits so we can DETECT 32-bit overflow instead of
    // triggering it (signed overflow is undefined behaviour in C++).
    const long long a = instruction.arg1.value;
    const long long b = instruction.arg2.value;
    long long result = 0;

    switch (instruction.opcode) {
        case Opcode::ADD: result = a + b; break;
        case Opcode::SUB: result = a - b; break;
        case Opcode::MUL: result = a * b; break;
        case Opcode::DIV:
            if (b == 0) {
                return std::nullopt;  // never fold division by zero
            }
            result = a / b;  // truncates toward zero, like the generated code will
            break;
        default:
            return std::nullopt;
    }

    if (result < INT_MIN || result > INT_MAX) {
        return std::nullopt;  // would overflow an int: leave it for runtime
    }
    return Instruction::copy(instruction.dest, Operand::constant(static_cast<int>(result)));
}

std::optional<Instruction> trySimplifyAlgebra(const Instruction& instruction) {
    if (!instruction.isBinary()) {
        return std::nullopt;
    }
    const Operand& left = instruction.arg1;
    const Operand& right = instruction.arg2;

    // Two constants: constant folding handles it (and handles it safely).
    if (left.isConstant() && right.isConstant()) {
        return std::nullopt;
    }

    const auto isConstant = [](const Operand& operand, int value) {
        return operand.isConstant() && operand.value == value;
    };
    const auto copyOf = [&instruction](const Operand& source) {
        return Instruction::copy(instruction.dest, source);
    };

    switch (instruction.opcode) {
        case Opcode::ADD:
            if (isConstant(right, 0)) return copyOf(left);   // x + 0
            if (isConstant(left, 0)) return copyOf(right);   // 0 + x
            break;
        case Opcode::SUB:
            if (isConstant(right, 0)) return copyOf(left);   // x - 0
            break;
        case Opcode::MUL:
            if (isConstant(right, 1)) return copyOf(left);   // x * 1
            if (isConstant(left, 1)) return copyOf(right);   // 1 * x
            if (isConstant(right, 0) || isConstant(left, 0)) {
                return copyOf(Operand::constant(0));          // x * 0, 0 * x
            }
            break;
        case Opcode::DIV:
            if (isConstant(right, 1)) return copyOf(left);   // x / 1
            break;                                            // x / 0: no rule
        default:
            break;
    }
    return std::nullopt;
}

std::vector<Change> propagateConstants(TacProgram& program) {
    std::vector<Change> changes;
    // name -> the constant it currently holds
    std::unordered_map<std::string, int> known;

    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        Instruction& instruction = program.instructions[i];
        const std::string before = formatInstruction(instruction);

        // Step 1: replace inputs that are known constants.
        bool replaced = false;
        const auto substitute = [&known, &replaced](Operand& operand) {
            if (!isNamed(operand)) {
                return;
            }
            const auto found = known.find(operandKey(operand));
            if (found != known.end()) {
                operand = Operand::constant(found->second);
                replaced = true;
            }
        };
        substitute(instruction.arg1);
        if (instruction.isBinary()) {
            substitute(instruction.arg2);
        }
        if (replaced) {
            changes.push_back(Change{"constant_propagation", i, before,
                                     formatInstruction(instruction)});
        }

        // Step 2: update what we know about the destination.
        if (instruction.opcode == Opcode::PRINT) {
            continue;  // print defines nothing
        }
        if (instruction.opcode == Opcode::COPY && instruction.arg1.isConstant()) {
            known[operandKey(instruction.dest)] = instruction.arg1.value;
        } else {
            known.erase(operandKey(instruction.dest));  // value no longer known
        }
    }
    return changes;
}

std::vector<Change> propagateCopies(TacProgram& program) {
    std::vector<Instruction>& instructions = program.instructions;
    std::vector<Change> changes;
    std::vector<bool> keep(instructions.size(), true);

    // name -> the (named) operand it is currently a copy of
    std::unordered_map<std::string, Operand> copyOf;

    for (std::size_t i = 0; i < instructions.size(); ++i) {
        Instruction& instruction = instructions[i];
        const std::string before = formatInstruction(instruction);

        // Step 1: read through known copies.
        bool replaced = false;
        const auto substitute = [&copyOf, &replaced](Operand& operand) {
            if (!isNamed(operand)) {
                return;
            }
            const auto found = copyOf.find(operandKey(operand));
            if (found != copyOf.end()) {
                operand = found->second;
                replaced = true;
            }
        };
        substitute(instruction.arg1);
        if (instruction.isBinary()) {
            substitute(instruction.arg2);
        }
        if (replaced) {
            changes.push_back(Change{"copy_propagation", i, before,
                                     formatInstruction(instruction)});
        }

        if (instruction.opcode == Opcode::PRINT) {
            continue;  // print defines nothing
        }

        const std::string destKey = operandKey(instruction.dest);

        // A copy of a name to itself changes nothing: drop it.
        if (instruction.opcode == Opcode::COPY && isNamed(instruction.arg1) &&
            operandKey(instruction.arg1) == destKey) {
            keep[i] = false;
            changes.push_back(Change{"copy_propagation", i,
                                     formatInstruction(instruction), "(removed)"});
            continue;
        }

        // Step 2: the destination gets a new value, so every fact that
        // mentions it (as the copy, or as the original) is now stale.
        copyOf.erase(destKey);
        for (auto it = copyOf.begin(); it != copyOf.end();) {
            if (operandKey(it->second) == destKey) {
                it = copyOf.erase(it);
            } else {
                ++it;
            }
        }

        // Step 3: remember a new copy.
        if (instruction.opcode == Opcode::COPY && isNamed(instruction.arg1)) {
            copyOf[destKey] = instruction.arg1;
        }
    }

    if (changes.empty()) {
        return changes;
    }
    std::vector<Instruction> remaining;
    for (std::size_t i = 0; i < instructions.size(); ++i) {
        if (keep[i]) {
            remaining.push_back(instructions[i]);
        }
    }
    instructions = std::move(remaining);
    return changes;
}

namespace {

// "result = left op right" is currently available in 'holder'.
struct AvailableExpression {
    Opcode opcode;
    Operand left;
    Operand right;
    Operand holder;
};

bool isCommutative(Opcode opcode) {
    return opcode == Opcode::ADD || opcode == Opcode::MUL;
}

bool computesSame(const AvailableExpression& available, const Instruction& instruction) {
    if (available.opcode != instruction.opcode) {
        return false;
    }
    if (available.left == instruction.arg1 && available.right == instruction.arg2) {
        return true;
    }
    // a + b and b + a are the same value; a - b and b - a are not.
    return isCommutative(instruction.opcode) && available.left == instruction.arg2 &&
           available.right == instruction.arg1;
}

}  // namespace

std::vector<Change> eliminateCommonSubexpressions(TacProgram& program) {
    std::vector<Change> changes;
    std::vector<AvailableExpression> available;

    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        Instruction& instruction = program.instructions[i];
        if (instruction.opcode == Opcode::PRINT) {
            continue;
        }

        // Look for an earlier computation of the same value BEFORE this
        // instruction changes anything.
        const Operand* reuse = nullptr;
        if (instruction.isBinary()) {
            for (const AvailableExpression& entry : available) {
                if (computesSame(entry, instruction)) {
                    reuse = &entry.holder;
                    break;
                }
            }
        }
        const std::optional<Operand> reusedHolder =
            reuse != nullptr ? std::optional<Operand>(*reuse) : std::nullopt;

        // This instruction gives 'dest' a new value: forget every available
        // expression that is stored in it or computed from it.
        const Operand dest = instruction.dest;
        for (auto it = available.begin(); it != available.end();) {
            if (it->holder == dest || it->left == dest || it->right == dest) {
                it = available.erase(it);
            } else {
                ++it;
            }
        }

        if (!instruction.isBinary()) {
            continue;  // copies and input() never become available expressions
        }

        if (reusedHolder) {
            const Instruction replacement = Instruction::copy(dest, *reusedHolder);
            changes.push_back(Change{"common_subexpression_elimination", i,
                                     formatInstruction(instruction),
                                     formatInstruction(replacement)});
            instruction = replacement;
        } else if (instruction.arg1 != dest && instruction.arg2 != dest) {
            // (If the result overwrites one of its own inputs, the stored
            // value no longer matches those inputs, so do not remember it.)
            available.push_back(AvailableExpression{instruction.opcode, instruction.arg1,
                                                    instruction.arg2, dest});
        }
    }
    return changes;
}

std::vector<Change> eliminateDeadCode(TacProgram& program) {
    std::vector<Instruction>& instructions = program.instructions;
    std::vector<bool> keep(instructions.size(), true);
    std::vector<Change> removed;
    std::unordered_set<std::string> live;  // names that are still needed

    const auto markUsed = [&live](const Operand& operand) {
        if (isNamed(operand)) {
            live.insert(operandKey(operand));
        }
    };

    // Bottom to top: an instruction is needed only if something BELOW it
    // reads what it writes.
    for (std::size_t k = instructions.size(); k-- > 0;) {
        const Instruction& instruction = instructions[k];

        if (instruction.opcode == Opcode::PRINT) {
            markUsed(instruction.arg1);
            continue;
        }

        const bool destinationNeeded = live.count(operandKey(instruction.dest)) != 0;
        if (!destinationNeeded && !hasSideEffect(instruction)) {
            keep[k] = false;
            removed.push_back(Change{"dead_code_elimination", k,
                                     formatInstruction(instruction), "(removed)"});
            continue;  // a removed instruction needs none of its inputs
        }

        // Kept: it writes its destination (so earlier writes to the same
        // name are no longer needed) and reads its inputs.
        live.erase(operandKey(instruction.dest));
        markUsed(instruction.arg1);
        if (instruction.isBinary()) {
            markUsed(instruction.arg2);
        }
    }

    if (removed.empty()) {
        return {};
    }

    std::vector<Instruction> remaining;
    for (std::size_t k = 0; k < instructions.size(); ++k) {
        if (keep[k]) {
            remaining.push_back(instructions[k]);
        }
    }
    instructions = std::move(remaining);

    // We collected bottom-to-top; report top-to-bottom.
    return std::vector<Change>(removed.rbegin(), removed.rend());
}
