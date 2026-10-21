// ast.h - Abstract Syntax Tree node classes.
//
// The AST is a tree that keeps only the logical structure of the program:
// no semicolons, no parentheses (the tree shape already encodes grouping).
//
// Every node is owned by a std::unique_ptr held by its parent, so the whole
// tree is freed automatically when the root is destroyed (RAII).
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Visitor interface.
// A visitor is an object with one function per node type. Each node's
// accept() calls the matching function, so new operations on the tree
// (printing, type checking, code generation...) can be added without
// touching the node classes.
// ---------------------------------------------------------------------------
class NumberExpr;
class VariableExpr;
class InputExpr;
class BinaryExpr;
class DeclarationStmt;
class AssignmentStmt;
class PrintStmt;
class Program;

class AstVisitor {
public:
    virtual ~AstVisitor() = default;
    virtual void visit(const NumberExpr& node) = 0;
    virtual void visit(const VariableExpr& node) = 0;
    virtual void visit(const InputExpr& node) = 0;
    virtual void visit(const BinaryExpr& node) = 0;
    virtual void visit(const DeclarationStmt& node) = 0;
    virtual void visit(const AssignmentStmt& node) = 0;
    virtual void visit(const PrintStmt& node) = 0;
    virtual void visit(const Program& node) = 0;
};

// ---------------------------------------------------------------------------
// Base class: every node remembers where it appeared in the source.
// ---------------------------------------------------------------------------
class ASTNode {
public:
    ASTNode(int line, int column) : line_(line), column_(column) {}
    virtual ~ASTNode() = default;

    // Nodes own children through unique_ptr, so copying is forbidden.
    ASTNode(const ASTNode&) = delete;
    ASTNode& operator=(const ASTNode&) = delete;

    virtual void accept(AstVisitor& visitor) const = 0;

    int line() const { return line_; }
    int column() const { return column_; }

private:
    int line_;
    int column_;
};

// ---------------------------------------------------------------------------
// Expressions: things that produce a value.
// ---------------------------------------------------------------------------
class Expr : public ASTNode {
public:
    using ASTNode::ASTNode;
};
using ExprPtr = std::unique_ptr<Expr>;

class NumberExpr : public Expr {
public:
    NumberExpr(int value, int line, int column)
        : Expr(line, column), value_(value) {}
    void accept(AstVisitor& v) const override { v.visit(*this); }
    int value() const { return value_; }

private:
    int value_;
};

class VariableExpr : public Expr {
public:
    VariableExpr(std::string name, int line, int column)
        : Expr(line, column), name_(std::move(name)) {}
    void accept(AstVisitor& v) const override { v.visit(*this); }
    const std::string& name() const { return name_; }

private:
    std::string name_;
};

// input(): reads one integer from the program's input at RUN time.
// Its value is unknown at compile time, and every evaluation reads the
// next value, so two input() calls are never interchangeable.
class InputExpr : public Expr {
public:
    InputExpr(int line, int column) : Expr(line, column) {}
    void accept(AstVisitor& v) const override { v.visit(*this); }
};

// The four arithmetic operators of the language.
enum class BinaryOp { ADD, SUB, MUL, DIV };

// Operator as source text: "+", "-", "*", "/".
inline const char* binaryOpSymbol(BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD: return "+";
        case BinaryOp::SUB: return "-";
        case BinaryOp::MUL: return "*";
        case BinaryOp::DIV: return "/";
    }
    return "?";  // unreachable
}

class BinaryExpr : public Expr {
public:
    BinaryExpr(BinaryOp op, ExprPtr left, ExprPtr right, int line, int column)
        : Expr(line, column),
          op_(op),
          left_(std::move(left)),
          right_(std::move(right)) {}
    void accept(AstVisitor& v) const override { v.visit(*this); }
    BinaryOp op() const { return op_; }
    const Expr& left() const { return *left_; }
    const Expr& right() const { return *right_; }

private:
    BinaryOp op_;
    ExprPtr left_;
    ExprPtr right_;
};

// ---------------------------------------------------------------------------
// Statements: things that do something.
// ---------------------------------------------------------------------------
class Stmt : public ASTNode {
public:
    using ASTNode::ASTNode;
};
using StmtPtr = std::unique_ptr<Stmt>;

// int x = <initializer>;
class DeclarationStmt : public Stmt {
public:
    DeclarationStmt(std::string name, ExprPtr initializer, int line, int column)
        : Stmt(line, column),
          name_(std::move(name)),
          initializer_(std::move(initializer)) {}
    void accept(AstVisitor& v) const override { v.visit(*this); }
    const std::string& name() const { return name_; }
    const Expr& initializer() const { return *initializer_; }

private:
    std::string name_;
    ExprPtr initializer_;
};

// x = <value>;
class AssignmentStmt : public Stmt {
public:
    AssignmentStmt(std::string name, ExprPtr value, int line, int column)
        : Stmt(line, column),
          name_(std::move(name)),
          value_(std::move(value)) {}
    void accept(AstVisitor& v) const override { v.visit(*this); }
    const std::string& name() const { return name_; }
    const Expr& value() const { return *value_; }

private:
    std::string name_;
    ExprPtr value_;
};

// print(<argument>);
class PrintStmt : public Stmt {
public:
    PrintStmt(ExprPtr argument, int line, int column)
        : Stmt(line, column), argument_(std::move(argument)) {}
    void accept(AstVisitor& v) const override { v.visit(*this); }
    const Expr& argument() const { return *argument_; }

private:
    ExprPtr argument_;
};

// ---------------------------------------------------------------------------
// The root of the tree: the whole program is a list of statements.
// ---------------------------------------------------------------------------
class Program : public ASTNode {
public:
    Program() : ASTNode(1, 1) {}
    void accept(AstVisitor& v) const override { v.visit(*this); }

    void addStatement(StmtPtr statement) {
        statements_.push_back(std::move(statement));
    }
    const std::vector<StmtPtr>& statements() const { return statements_; }

private:
    std::vector<StmtPtr> statements_;
};
