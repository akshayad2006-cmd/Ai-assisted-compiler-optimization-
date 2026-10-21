// symbol_table.h - records every declared variable.
//
// A SYMBOL TABLE answers "is this name declared, and what is its type?".
// It is organised as a stack of scopes; lookups search from the innermost
// scope outwards. The language has no blocks yet, so only the global scope
// (the bottom of the stack) is used.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// INVALID marks an expression whose type could not be determined because an
// error was already reported. It prevents a cascade of follow-up errors.
enum class Type { INT, INVALID };

inline const char* typeName(Type type) {
    switch (type) {
        case Type::INT:     return "int";
        case Type::INVALID: return "<invalid>";
    }
    return "<unknown>";  // unreachable
}

struct Symbol {
    std::string name;
    Type type;
    int line;    // where it was declared
    int column;
};

class SymbolTable {
public:
    SymbolTable();  // starts with the global scope

    void enterScope();
    void exitScope();  // the global scope is never removed

    // Adds a symbol to the current scope.
    // Returns false (and changes nothing) if the name already exists there.
    bool declare(const Symbol& symbol);

    // Searches the current scope only.
    std::optional<Symbol> lookupInCurrentScope(const std::string& name) const;

    // Searches from the innermost scope outwards.
    std::optional<Symbol> lookup(const std::string& name) const;

    // Symbols of the global scope in declaration order (for printing).
    std::vector<Symbol> globalSymbols() const;

private:
    struct Scope {
        std::vector<Symbol> ordered;                      // keeps declaration order
        std::unordered_map<std::string, std::size_t> index;  // name -> position
    };
    std::vector<Scope> scopes_;
};

// Human-readable dump of the global scope, one symbol per line.
std::string formatSymbolTable(const SymbolTable& table);
