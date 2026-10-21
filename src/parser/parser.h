// parser.h - public interface of the syntax analyzer.
//
// SYNTAX ANALYSIS checks that the tokens follow the grammar of the language
// and builds the Abstract Syntax Tree (AST).
#pragma once

#include <memory>
#include <vector>

#include "ast/ast.h"
#include "common/diagnostic.h"
#include "lexer/lexer.h"

struct ParseResult {
    // The tree. Always non-null after Parser::parse (it may be empty or
    // partial when errors occurred; do not use it unless ok()).
    std::unique_ptr<Program> program;
    std::vector<Diagnostic> errors;  // syntax errors (empty on success)

    bool ok() const { return errors.empty(); }
};

class Parser {
public:
    // Parses a token list produced by Lexer::tokenize.
    // Never throws; syntax errors are reported in ParseResult::errors.
    static ParseResult parse(const std::vector<Token>& tokens);
};
