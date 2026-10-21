// token_stream.h - gives the parser the lexer's tokens one at a time.
// The vector must be non-empty and end with an END_OF_FILE token
// (Lexer::tokenize always guarantees that).
#pragma once

#include <cstddef>
#include <vector>

#include "lexer/lexer.h"

class TokenStream {
public:
    explicit TokenStream(const std::vector<Token>& tokens) : tokens_(tokens) {}

    // Returns the next token. Once the end-of-file token is reached,
    // it keeps returning it, so the parser can never run off the end.
    const Token& next() {
        const Token& current = tokens_[position_];
        if (position_ + 1 < tokens_.size()) {
            ++position_;
        }
        return current;
    }

private:
    const std::vector<Token>& tokens_;
    std::size_t position_ = 0;
};
