#include "semantic/semantic_analyzer.h"

#include <string>
#include <utility>

namespace {

// A visitor that checks the tree in source order.
// After visiting an expression, exprType_ holds that expression's type.
class Analyzer : public AstVisitor {
public:
    SemanticResult run(const Program& program) {
        program.accept(*this);
        SemanticResult result;
        result.errors = std::move(errors_);
        result.symbols = std::move(symbols_);
        return result;
    }

    void visit(const NumberExpr&) override {
        exprType_ = Type::INT;
    }

    void visit(const VariableExpr& node) override {
        const auto symbol = symbols_.lookup(node.name());
        if (!symbol) {
            error(node, "Variable '" + node.name() + "' is not declared.");
            exprType_ = Type::INVALID;
        } else {
            exprType_ = symbol->type;
        }
    }

    void visit(const InputExpr&) override {
        exprType_ = Type::INT;  // input() always produces an int
    }

    void visit(const BinaryExpr& node) override {
        // Visit BOTH sides so errors in either operand are reported.
        node.left().accept(*this);
        const Type left = exprType_;
        node.right().accept(*this);
        const Type right = exprType_;

        // Every operator works on ints and produces an int. If an operand
        // was already reported as invalid, the result is invalid too (and
        // we stay silent: the real error was reported already).
        exprType_ = (left == Type::INT && right == Type::INT) ? Type::INT
                                                              : Type::INVALID;
    }

    void visit(const DeclarationStmt& node) override {
        // Analyse the initializer BEFORE declaring the name, so that
        // "int x = x + 1;" reports x as undeclared.
        node.initializer().accept(*this);
        const Type valueType = exprType_;

        const Symbol symbol{node.name(), Type::INT, node.line(), node.column()};
        if (!symbols_.declare(symbol)) {
            const auto first = symbols_.lookupInCurrentScope(node.name());
            error(node, "Variable '" + node.name() + "' already declared" +
                            (first ? " (first declared at line " +
                                         std::to_string(first->line) + ")"
                                   : std::string()) +
                            ".");
            return;
        }
        checkAssignable(symbol.type, valueType, node.name(), node);
    }

    void visit(const AssignmentStmt& node) override {
        node.value().accept(*this);
        const Type valueType = exprType_;

        const auto symbol = symbols_.lookup(node.name());
        if (!symbol) {
            error(node, "Variable '" + node.name() + "' is not declared.");
            return;
        }
        checkAssignable(symbol->type, valueType, node.name(), node);
    }

    void visit(const PrintStmt& node) override {
        node.argument().accept(*this);
    }

    void visit(const Program& node) override {
        for (const StmtPtr& statement : node.statements()) {
            statement->accept(*this);
        }
    }

private:
    void error(const ASTNode& node, const std::string& message) {
        errors_.push_back(Diagnostic{node.line(), node.column(), message});
    }

    // With only 'int' this can never fail yet; it is the place where a
    // second type (bool, ...) would be checked.
    void checkAssignable(Type target, Type value, const std::string& name,
                         const ASTNode& node) {
        if (target == Type::INVALID || value == Type::INVALID) {
            return;  // an error was reported already
        }
        if (target != value) {
            error(node, std::string("Cannot assign a value of type '") +
                            typeName(value) + "' to variable '" + name +
                            "' of type '" + typeName(target) + "'.");
        }
    }

    SymbolTable symbols_;
    std::vector<Diagnostic> errors_;
    Type exprType_ = Type::INVALID;
};

}  // namespace

SemanticResult SemanticAnalyzer::analyze(const Program& program) {
    Analyzer analyzer;
    return analyzer.run(program);
}
