#include "lexer/lexer.h"

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::INT:         return "INT";
        case TokenType::PRINT:       return "PRINT";
        case TokenType::INPUT:       return "INPUT";
        case TokenType::IDENTIFIER:  return "IDENTIFIER";
        case TokenType::NUMBER:      return "NUMBER";
        case TokenType::ASSIGN:      return "ASSIGN";
        case TokenType::PLUS:        return "PLUS";
        case TokenType::MINUS:       return "MINUS";
        case TokenType::STAR:        return "STAR";
        case TokenType::SLASH:       return "SLASH";
        case TokenType::LPAREN:      return "LPAREN";
        case TokenType::RPAREN:      return "RPAREN";
        case TokenType::SEMICOLON:   return "SEMICOLON";
        case TokenType::LBRACE:      return "LBRACE";
        case TokenType::RBRACE:      return "RBRACE";
    }
    return "UNKNOWN";  // unreachable, keeps the compiler happy
}

std::string tokenToString(const Token& token) {
    std::string result = tokenTypeName(token.type);
    // Only identifiers and numbers carry extra information worth showing.
    if (token.type == TokenType::IDENTIFIER || token.type == TokenType::NUMBER) {
        result += "(" + token.text + ")";
    }
    return result;
}
