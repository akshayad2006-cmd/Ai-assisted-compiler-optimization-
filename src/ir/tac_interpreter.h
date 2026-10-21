// tac_interpreter.h - Straight-line TAC interpreter for differential runtime verification.
//
// Simulates the execution of a Three Address Code program with a stream of integer
// inputs, tracking printed outputs, variable environments, and runtime faults.
// Used by the validator to compare the observable runtime behavior of the original
// program against an optimized program.
//
// SAFETY NOTE:
// Sampled runtime equivalence is empirical testing evidence over selected test inputs.
// It is NOT a formal mathematical proof of universal semantic equivalence.
#pragma once

#include <climits>
#include <cstddef>
#include <string>
#include <vector>

#include "ir/tac.h"

enum class ExecutionFault {
    NONE,
    DIVISION_BY_ZERO,
    OVERFLOW_FAULT,
    INPUT_EXHAUSTED,
    UNDEFINED_OPERAND
};

struct ExecutionResult {
    ExecutionFault fault = ExecutionFault::NONE;
    std::string faultMessage;
    std::vector<int> outputs;
    std::size_t inputsConsumed = 0;
    std::size_t instructionsExecuted = 0;

    bool ok() const { return fault == ExecutionFault::NONE; }
    bool operator==(const ExecutionResult& other) const;
    bool operator!=(const ExecutionResult& other) const { return !(*this == other); }
};

struct DifferentialComparison {
    bool equivalent = true;
    std::size_t testsRun = 0;
    std::string mismatchReason;
};

class TacInterpreter {
public:
    // Executes 'program' sequentially given 'inputs'.
    static ExecutionResult run(const TacProgram& program, const std::vector<int>& inputs);

    // Compares runtime results of 'original' and 'optimized' across 'testSuites'.
    // Returns equivalent = true if all test cases produce matching outputs and fault states.
    static DifferentialComparison compare(
        const TacProgram& original,
        const TacProgram& optimized,
        const std::vector<std::vector<int>>& testSuites);

    // Generates a default deterministic set of test input vectors based on the number
    // of input() operations present in the program.
    static std::vector<std::vector<int>> defaultTestVectors(std::size_t inputCount);
};

std::string faultToString(ExecutionFault fault);
