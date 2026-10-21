// tac_generator.h - converts a checked AST into Three Address Code.
#pragma once

#include "ast/ast.h"
#include "ir/tac.h"

class TacGenerator {
public:
    // Precondition: the program passed semantic analysis (every variable
    // is declared). Never throws.
    static TacProgram generate(const Program& program);
};
