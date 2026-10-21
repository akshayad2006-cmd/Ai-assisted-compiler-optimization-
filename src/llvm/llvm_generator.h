#pragma once

#include <string>

#include "ir/tac.h"

// Converts a TAC program into textual LLVM IR.
// Returns an empty string and sets error when generation fails.
std::string generateLlvmIr(const TacProgram& program, std::string& error);
