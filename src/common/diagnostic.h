// diagnostic.h - a single error/warning message with its source position.
// Every compiler stage (lexer, parser, semantic analysis...) reuses this.
#pragma once

#include <string>

struct Diagnostic {
    int line;             // 1-based line number
    int column;           // 1-based column number
    std::string message;  // human-readable description
};
