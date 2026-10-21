#include "ast/ast_printer.h"

#include <sstream>
#include <vector>

namespace {

// A visitor that writes the tree into a string stream.
class AstPrinter : public AstVisitor {
public:
    std::string result() const { return out_.str(); }

    void visit(const NumberExpr& node) override {
        out_ << node.value() << "\n";
    }

    void visit(const VariableExpr& node) override {
        out_ << node.name() << "\n";
    }

    void visit(const InputExpr&) override {
        out_ << "input()\n";
    }

    void visit(const BinaryExpr& node) override {
        out_ << binaryOpSymbol(node.op()) << "\n";
        printChildren({&node.left(), &node.right()});
    }

    void visit(const DeclarationStmt& node) override {
        out_ << "Declaration " << node.name() << "\n";
        printChildren({&node.initializer()});
    }

    void visit(const AssignmentStmt& node) override {
        out_ << "Assignment " << node.name() << "\n";
        printChildren({&node.value()});
    }

    void visit(const PrintStmt& node) override {
        out_ << "Print\n";
        printChildren({&node.argument()});
    }

    void visit(const Program& node) override {
        out_ << "Program\n";
        std::vector<const ASTNode*> children;
        for (const StmtPtr& statement : node.statements()) {
            children.push_back(statement.get());
        }
        printChildren(children);
    }

private:
    // Prints each child on its own line. The last child uses "└── " and
    // everything below it is indented with spaces; the others use "├── "
    // and keep a vertical line "│   " going down.
    void printChildren(const std::vector<const ASTNode*>& children) {
        for (std::size_t i = 0; i < children.size(); ++i) {
            const bool isLast = (i + 1 == children.size());
            out_ << prefix_ << (isLast ? "└── " : "├── ");

            const std::string savedPrefix = prefix_;
            prefix_ += isLast ? "    " : "│   ";
            children[i]->accept(*this);
            prefix_ = savedPrefix;
        }
    }

    std::ostringstream out_;
    std::string prefix_;
};

}  // namespace

std::string printAst(const Program& program) {
    AstPrinter printer;
    program.accept(printer);
    return printer.result();
}
