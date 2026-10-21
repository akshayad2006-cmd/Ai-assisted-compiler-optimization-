#include "ir/tac_interpreter.h"

#include <climits>
#include <sstream>
#include <unordered_map>

namespace {

std::string operandKey(const Operand& operand) {
    return (operand.isTemporary() ? "t:" : "v:") + operand.name;
}

std::string formatInputs(const std::vector<int>& inputs) {
    std::ostringstream ss;
    ss << "[";
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << inputs[i];
    }
    ss << "]";
    return ss.str();
}

std::string formatOutputs(const std::vector<int>& outputs) {
    std::ostringstream ss;
    ss << "[";
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << outputs[i];
    }
    ss << "]";
    return ss.str();
}

}  // namespace

std::string faultToString(ExecutionFault fault) {
    switch (fault) {
        case ExecutionFault::NONE: return "NONE";
        case ExecutionFault::DIVISION_BY_ZERO: return "DIVISION_BY_ZERO";
        case ExecutionFault::OVERFLOW_FAULT: return "OVERFLOW_FAULT";
        case ExecutionFault::INPUT_EXHAUSTED: return "INPUT_EXHAUSTED";
        case ExecutionFault::UNDEFINED_OPERAND: return "UNDEFINED_OPERAND";
    }
    return "UNKNOWN";
}

bool ExecutionResult::operator==(const ExecutionResult& other) const {
    return fault == other.fault &&
           outputs == other.outputs &&
           inputsConsumed == other.inputsConsumed;
}

ExecutionResult TacInterpreter::run(const TacProgram& program, const std::vector<int>& inputs) {
    ExecutionResult result;
    std::unordered_map<std::string, int> env;
    std::size_t inputCursor = 0;

    const auto resolve = [&](const Operand& operand, int& out) -> bool {
        if (operand.isConstant()) {
            out = operand.value;
            return true;
        }
        if (operand.isVariable() || operand.isTemporary()) {
            const auto it = env.find(operandKey(operand));
            if (it != env.end()) {
                out = it->second;
                return true;
            }
            result.fault = ExecutionFault::UNDEFINED_OPERAND;
            result.faultMessage = "read from undefined operand '" + operandToString(operand) + "'";
            return false;
        }
        result.fault = ExecutionFault::UNDEFINED_OPERAND;
        result.faultMessage = "invalid operand kind";
        return false;
    };

    for (const Instruction& inst : program.instructions) {
        result.instructionsExecuted++;

        if (inst.opcode == Opcode::INPUT) {
            if (inputCursor >= inputs.size()) {
                result.fault = ExecutionFault::INPUT_EXHAUSTED;
                result.faultMessage = "program attempted input() but input stream was exhausted";
                break;
            }
            env[operandKey(inst.dest)] = inputs[inputCursor++];
            result.inputsConsumed = inputCursor;
            continue;
        }

        if (inst.opcode == Opcode::PRINT) {
            int val = 0;
            if (!resolve(inst.arg1, val)) {
                break;
            }
            result.outputs.push_back(val);
            continue;
        }

        if (inst.opcode == Opcode::COPY) {
            int val = 0;
            if (!resolve(inst.arg1, val)) {
                break;
            }
            env[operandKey(inst.dest)] = val;
            continue;
        }

        // Binary instructions
        int left = 0;
        int right = 0;
        if (!resolve(inst.arg1, left) || !resolve(inst.arg2, right)) {
            break;
        }

        long long computed = 0;
        switch (inst.opcode) {
            case Opcode::ADD:
                computed = static_cast<long long>(left) + right;
                if (computed < INT_MIN || computed > INT_MAX) {
                    result.fault = ExecutionFault::OVERFLOW_FAULT;
                    result.faultMessage = "signed 32-bit integer overflow in addition";
                }
                break;
            case Opcode::SUB:
                computed = static_cast<long long>(left) - right;
                if (computed < INT_MIN || computed > INT_MAX) {
                    result.fault = ExecutionFault::OVERFLOW_FAULT;
                    result.faultMessage = "signed 32-bit integer overflow in subtraction";
                }
                break;
            case Opcode::MUL:
                computed = static_cast<long long>(left) * right;
                if (computed < INT_MIN || computed > INT_MAX) {
                    result.fault = ExecutionFault::OVERFLOW_FAULT;
                    result.faultMessage = "signed 32-bit integer overflow in multiplication";
                }
                break;
            case Opcode::DIV:
                if (right == 0) {
                    result.fault = ExecutionFault::DIVISION_BY_ZERO;
                    result.faultMessage = "division by zero";
                } else if (left == INT_MIN && right == -1) {
                    result.fault = ExecutionFault::OVERFLOW_FAULT;
                    result.faultMessage = "integer overflow in division (INT_MIN / -1)";
                } else {
                    computed = left / right;
                }
                break;
            default:
                break;
        }

        if (result.fault != ExecutionFault::NONE) {
            break;
        }
        env[operandKey(inst.dest)] = static_cast<int>(computed);
    }

    return result;
}

DifferentialComparison TacInterpreter::compare(
    const TacProgram& original,
    const TacProgram& optimized,
    const std::vector<std::vector<int>>& testSuites) {

    DifferentialComparison comparison;
    comparison.testsRun = testSuites.size();

    for (std::size_t i = 0; i < testSuites.size(); ++i) {
        const std::vector<int>& inputs = testSuites[i];
        const ExecutionResult origRes = run(original, inputs);
        const ExecutionResult optRes = run(optimized, inputs);

        if (origRes != optRes) {
            comparison.equivalent = false;
            std::ostringstream ss;
            ss << "mismatch on test input " << formatInputs(inputs) << ": ";
            if (origRes.fault != optRes.fault) {
                ss << "fault status differed (original: " << faultToString(origRes.fault)
                   << ", optimized: " << faultToString(optRes.fault) << ")";
            } else if (origRes.inputsConsumed != optRes.inputsConsumed) {
                ss << "inputs consumed differed (original: " << origRes.inputsConsumed
                   << ", optimized: " << optRes.inputsConsumed << ")";
            } else {
                ss << "outputs differed (original: " << formatOutputs(origRes.outputs)
                   << ", optimized: " << formatOutputs(optRes.outputs) << ")";
            }
            comparison.mismatchReason = ss.str();
            return comparison;
        }
    }

    comparison.equivalent = true;
    return comparison;
}

std::vector<std::vector<int>> TacInterpreter::defaultTestVectors(std::size_t inputCount) {
    if (inputCount == 0) {
        return {{}};
    }
    if (inputCount == 1) {
        return {
            {0}, {1}, {-1}, {2}, {5}, {-10}, {42}, {100}, {-999}
        };
    }
    if (inputCount == 2) {
        return {
            {0, 0}, {1, 1}, {10, 2}, {20, 5}, {7, 3},
            {-5, 3}, {0, 5}, {10, -1}, {-10, -2}, {100, 0},
            {42, 1}, {1, 0}, {-4, 2}, {15, 15}
        };
    }
    // 3 or more inputs
    std::vector<std::vector<int>> suites;
    const std::vector<int> sampleVals = {0, 1, 2, -1, 5, 10};
    for (int v1 : {0, 2, -1}) {
        for (int v2 : {1, 0, 5}) {
            std::vector<int> vec(inputCount, 1);
            vec[0] = v1;
            if (inputCount > 1) vec[1] = v2;
            suites.push_back(vec);
        }
    }
    return suites;
}
