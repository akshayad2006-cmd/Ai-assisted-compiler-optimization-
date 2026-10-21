#include "symbol_table/symbol_table.h"

#include <sstream>

SymbolTable::SymbolTable() {
    scopes_.emplace_back();  // the global scope
}

void SymbolTable::enterScope() {
    scopes_.emplace_back();
}

void SymbolTable::exitScope() {
    if (scopes_.size() > 1) {
        scopes_.pop_back();
    }
}

bool SymbolTable::declare(const Symbol& symbol) {
    Scope& current = scopes_.back();
    if (current.index.count(symbol.name) != 0) {
        return false;
    }
    current.index[symbol.name] = current.ordered.size();
    current.ordered.push_back(symbol);
    return true;
}

std::optional<Symbol> SymbolTable::lookupInCurrentScope(const std::string& name) const {
    const Scope& current = scopes_.back();
    const auto found = current.index.find(name);
    if (found == current.index.end()) {
        return std::nullopt;
    }
    return current.ordered[found->second];
}

std::optional<Symbol> SymbolTable::lookup(const std::string& name) const {
    // Innermost scope first, so inner declarations hide outer ones.
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->index.find(name);
        if (found != scope->index.end()) {
            return scope->ordered[found->second];
        }
    }
    return std::nullopt;
}

std::vector<Symbol> SymbolTable::globalSymbols() const {
    return scopes_.front().ordered;
}

std::string formatSymbolTable(const SymbolTable& table) {
    std::ostringstream out;
    out << "Symbol table (global scope):\n";
    const std::vector<Symbol> symbols = table.globalSymbols();
    if (symbols.empty()) {
        out << "  (empty)\n";
    }
    for (const Symbol& symbol : symbols) {
        out << "  " << symbol.name << " : " << typeName(symbol.type)
            << " (declared at " << symbol.line << ":" << symbol.column << ")\n";
    }
    return out.str();
}
