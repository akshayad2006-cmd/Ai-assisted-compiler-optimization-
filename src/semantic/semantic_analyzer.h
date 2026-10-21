// semantic_analyzer.h - checks the MEANING of a program that is already
// grammatically correct: declared variables, no duplicates, matching types.
#pragma once

#include <vector>

#include "ast/ast.h"
#include "common/diagnostic.h"
#include "symbol_table/symbol_table.h"

struct SemanticResult {
    std::vector<Diagnostic> errors;  // empty when the program is valid
    SymbolTable symbols;             // global scope as it ended up

    bool ok() const { return errors.empty(); }
};

class SemanticAnalyzer {
public:
    // Walks the whole tree and reports every semantic error it finds.
    // Never throws; analysis continues after each error.
    static SemanticResult analyze(const Program& program);
};
