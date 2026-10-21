/* lexer.l - Flex specification for the mini-language. */

/* noyywrap: we scan one string only, no "next file" logic needed
   noinput/nounput: don't generate unused helper functions
   never-interactive: we never read from a terminal */
%option noyywrap nounput noinput never-interactive

%{
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "lexer/lexer.h"

namespace {

// Where we currently are in the source (1-based).
int g_line = 1;
int g_col = 1;
// Where the token being matched right now started.
int g_tokLine = 1;
int g_tokCol = 1;

// The token produced by the last emit() call.
Token g_current;
// Where lexical errors get stored while tokenize() runs.
std::vector<Diagnostic>* g_errors = nullptr;

// Record the matched token and return its type code to the scanning loop.
int emit(TokenType type, const char* text) {
    g_current = Token{type, std::string(text), g_tokLine, g_tokCol};
    return static_cast<int>(type);
}

void reportError(const std::string& message) {
    if (g_errors != nullptr) {
        g_errors->push_back(Diagnostic{g_tokLine, g_tokCol, message});
    }
}

// Printable characters are shown as 'c'; others as a hex byte value.
std::string describeChar(unsigned char c) {
    if (c >= 32 && c < 127) {
        return std::string("'") + static_cast<char>(c) + "'";
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "byte 0x%02X", static_cast<unsigned>(c));
    return std::string(buffer);
}

}  // namespace

/* Runs automatically BEFORE every rule's action:
   remember where this token starts, then advance the column. */
#define YY_USER_ACTION \
    g_tokLine = g_line; \
    g_tokCol = g_col;   \
    g_col += yyleng;
%}

%%

[ \t\r]+                      { /* skip whitespace */ }
\n                            { g_line++; g_col = 1; }
"//"[^\n]*                    { /* skip line comment */ }

"int"                         { return emit(TokenType::INT, yytext); }
"print"                       { return emit(TokenType::PRINT, yytext); }
"input"                       { return emit(TokenType::INPUT, yytext); }

[A-Za-z_][A-Za-z0-9_]*        { return emit(TokenType::IDENTIFIER, yytext); }

[0-9]+[A-Za-z_][A-Za-z0-9_]*  {
    reportError("invalid identifier '" + std::string(yytext) +
                "' (identifiers cannot start with a digit)");
}

[0-9]+                        {
    errno = 0;
    long value = std::strtol(yytext, nullptr, 10);
    if (errno == ERANGE || value > INT_MAX) {
        reportError("integer literal '" + std::string(yytext) + "' is too large");
    } else {
        return emit(TokenType::NUMBER, yytext);
    }
}

"="                           { return emit(TokenType::ASSIGN, yytext); }
"+"                           { return emit(TokenType::PLUS, yytext); }
"-"                           { return emit(TokenType::MINUS, yytext); }
"*"                           { return emit(TokenType::STAR, yytext); }
"/"                           { return emit(TokenType::SLASH, yytext); }
"("                           { return emit(TokenType::LPAREN, yytext); }
")"                           { return emit(TokenType::RPAREN, yytext); }
";"                           { return emit(TokenType::SEMICOLON, yytext); }
"{"                           { return emit(TokenType::LBRACE, yytext); }
"}"                           { return emit(TokenType::RBRACE, yytext); }

.                             {
    reportError("unexpected character " +
                describeChar(static_cast<unsigned char>(yytext[0])));
}

%%

/* ---- User code: the C++ wrapper around the generated scanner ---- */

LexResult Lexer::tokenize(const std::string& source) {
    LexResult result;

    // Reset position tracking for this run.
    g_line = 1;
    g_col = 1;
    g_errors = &result.errors;

    // Tell Flex to scan our string instead of a file.
    YY_BUFFER_STATE buffer = yy_scan_string(source.c_str());

    // yylex() returns a token code for each token and 0 at end of input.
    // Errors and whitespace do not return, so the scanner just continues.
    while (yylex() != 0) {
        result.tokens.push_back(g_current);
    }

    yy_delete_buffer(buffer);

    // Always finish with an explicit end-of-file token.
    result.tokens.push_back(Token{TokenType::END_OF_FILE, "", g_line, g_col});

    g_errors = nullptr;
    return result;
}
