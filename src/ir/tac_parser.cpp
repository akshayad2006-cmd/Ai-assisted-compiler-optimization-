#include "ir/tac_parser.h"

#include <cctype>
#include <climits>
#include <optional>
#include <sstream>

namespace {

bool isIdentifier(const std::string& word) {
    if (word.empty() || !(std::isalpha(static_cast<unsigned char>(word[0])) || word[0] == '_')) {
        return false;
    }
    for (const char c : word) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            return false;
        }
    }
    return true;
}

// "t" followed by one or more digits, e.g. t1, t42.
bool isTemporaryName(const std::string& word) {
    if (word.size() < 2 || word[0] != 't') {
        return false;
    }
    for (std::size_t i = 1; i < word.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(word[i]))) {
            return false;
        }
    }
    return true;
}

// A number such as 10 or -5. Returns nullopt if it is not a number or does
// not fit in a 32-bit int.
std::optional<int> parseNumber(const std::string& word) {
    std::size_t start = (!word.empty() && word[0] == '-') ? 1 : 0;
    if (start == word.size()) {
        return std::nullopt;
    }
    for (std::size_t i = start; i < word.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(word[i]))) {
            return std::nullopt;
        }
    }
    if (word.size() - start > 10) {
        return std::nullopt;  // far too long for an int
    }
    const long long value = std::stoll(word);
    if (value < INT_MIN || value > INT_MAX) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

// A name (variable or temporary), or nullopt if the word is not a valid name.
std::optional<Operand> parseName(const std::string& word) {
    if (!isIdentifier(word)) {
        return std::nullopt;
    }
    if (isTemporaryName(word)) {
        return Operand::temporary(std::stoi(word.substr(1)));
    }
    return Operand::variable(word);
}

// A value: number or name.
std::optional<Operand> parseValue(const std::string& word) {
    if (const auto number = parseNumber(word)) {
        return Operand::constant(*number);
    }
    return parseName(word);
}

std::optional<Opcode> parseOperator(const std::string& word) {
    if (word == "+") return Opcode::ADD;
    if (word == "-") return Opcode::SUB;
    if (word == "*") return Opcode::MUL;
    if (word == "/") return Opcode::DIV;
    return std::nullopt;
}

}  // namespace

TacParseResult parseTac(const std::string& text) {
    TacParseResult result;
    std::istringstream lines(text);
    std::string line;
    int lineNumber = 0;

    while (std::getline(lines, line)) {
        ++lineNumber;
        const auto fail = [&](const std::string& message) {
            result.errors.push_back(Diagnostic{lineNumber, 1, message});
        };

        std::istringstream splitter(line);
        std::vector<std::string> words;
        for (std::string word; splitter >> word;) {
            words.push_back(word);
        }
        if (words.empty() || words[0].compare(0, 2, "//") == 0) {
            continue;
        }

        // print <value>
        if (words[0] == "print") {
            if (words.size() != 2) {
                fail("expected 'print <value>'");
                continue;
            }
            const auto value = parseValue(words[1]);
            if (!value) {
                fail("invalid operand '" + words[1] + "'");
                continue;
            }
            result.program.instructions.push_back(Instruction::print(*value));
            continue;
        }

        // <name> = ...
        if (words.size() < 3 || words[1] != "=") {
            fail("expected '<name> = <value>', '<name> = <a> <op> <b>', "
                 "'<name> = input()' or 'print <value>'");
            continue;
        }
        const auto dest = parseName(words[0]);
        if (!dest) {
            fail("invalid destination '" + words[0] + "' (must be a variable or temporary name)");
            continue;
        }

        if (words.size() == 3 && words[2] == "input()") {
            result.program.instructions.push_back(Instruction::input(*dest));
        } else if (words.size() == 3) {
            const auto source = parseValue(words[2]);
            if (!source) {
                fail("invalid operand '" + words[2] + "'");
                continue;
            }
            result.program.instructions.push_back(Instruction::copy(*dest, *source));
        } else if (words.size() == 5) {
            const auto op = parseOperator(words[3]);
            const auto left = parseValue(words[2]);
            const auto right = parseValue(words[4]);
            if (!op) {
                fail("unknown operator '" + words[3] + "' (expected + - * /)");
            } else if (!left) {
                fail("invalid operand '" + words[2] + "'");
            } else if (!right) {
                fail("invalid operand '" + words[4] + "'");
            } else {
                result.program.instructions.push_back(Instruction::binary(*op, *dest, *left, *right));
            }
        } else {
            fail("wrong number of words on this line");
        }
    }
    return result;
}
