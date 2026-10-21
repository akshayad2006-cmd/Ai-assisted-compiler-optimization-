// lexer.h - public interface of the lexical analyzer.
//
// A TOKEN is the smallest meaningful unit of the source code,
// e.g. the keyword 'int', the number '10', or the symbol ';'.
#pragma once

#include <string>
#include <vector>

#include "common/diagnostic.h"

// All kinds of tokens our mini-language has.
// END_OF_FILE is 0 on purpose: Flex's yylex() returns 0 at end of input.
enum class TokenType : int {
    END_OF_FILE = 0,
    // keywords
    INT,
    PRINT,
    INPUT,
    // literals / names
    IDENTIFIER,
    NUMBER,
    // operators
    ASSIGN,  // =
    PLUS,    // +
    MINUS,   // -
    STAR,    // *
    SLASH,   // /
    // symbols
    LPAREN,     // (
    RPAREN,     // )
    SEMICOLON,  // ;
    LBRACE,     // {
    RBRACE      // }
};

struct Token {
    TokenType type = TokenType::END_OF_FILE;
    std::string text;  // the exact characters matched, e.g. "x" or "10"
    int line = 0;      // 1-based
    int column = 0;    // 1-based column of the first character
};

// Result of scanning a whole source text.
struct LexResult {
    std::vector<Token> tokens;       // always ends with an END_OF_FILE token
    std::vector<Diagnostic> errors;  // lexical errors found (may be empty)

    bool ok() const { return errors.empty(); }
};

// Name of a token type, e.g. TokenType::PLUS -> "PLUS".
const char* tokenTypeName(TokenType type);

// Debug text for a token: "IDENTIFIER(x)", "NUMBER(10)", "PLUS", ...
std::string tokenToString(const Token& token);

class Lexer {
public:
    // Scans the entire source text. Never throws; problems are reported in
    // LexResult::errors and scanning continues after each error.
    static LexResult tokenize(const std::string& source);
};
