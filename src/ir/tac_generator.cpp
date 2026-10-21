#include "ir/tac_generator.h"

#include <utility>

namespace {

// Maps an AST operator to the matching TAC opcode.
Opcode toOpcode(BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD: return Opcode::ADD;
        case BinaryOp::SUB: return Opcode::SUB;
        case BinaryOp::MUL: return Opcode::MUL;
        case BinaryOp::DIV: return Opcode::DIV;
    }
    return Opcode::ADD;  // unreachable
}

// A visitor that walks the tree and emits instructions.
//
// Convention: after visiting an EXPRESSION, 'result_' holds the operand
// that contains its value:
//   - a number           -> a constant operand (no instruction needed)
//   - a variable         -> a variable operand (no instruction needed)
//   - a binary operation -> a new temporary, filled by an emitted instruction
class Generator : public AstVisitor {
public:
    TacProgram run(const Program& program) {
        program.accept(*this);
        return std::move(program_);
    }

    void visit(const NumberExpr& node) override {
        result_ = Operand::constant(node.value());
    }

    void visit(const VariableExpr& node) override {
        result_ = Operand::variable(node.name());
    }

    void visit(const InputExpr&) override {
        // Every input() reads a new value, so it always gets its own
        // temporary and its own instruction. Order of emission = order of reads.
        const Operand temp = Operand::temporary(++tempCounter_);
        program_.instructions.push_back(Instruction::input(temp));
        result_ = temp;
    }

    void visit(const BinaryExpr& node) override {
        // Left operand first, then right: this fixes the evaluation order.
        node.left().accept(*this);
        const Operand left = result_;
        node.right().accept(*this);
        const Operand right = result_;

        const Operand temp = Operand::temporary(++tempCounter_);
        program_.instructions.push_back(
            Instruction::binary(toOpcode(node.op()), temp, left, right));
        result_ = temp;
    }

    void visit(const DeclarationStmt& node) override {
        node.initializer().accept(*this);
        program_.instructions.push_back(
            Instruction::copy(Operand::variable(node.name()), result_));
    }

    void visit(const AssignmentStmt& node) override {
        node.value().accept(*this);
        program_.instructions.push_back(
            Instruction::copy(Operand::variable(node.name()), result_));
    }

    void visit(const PrintStmt& node) override {
        node.argument().accept(*this);
        program_.instructions.push_back(Instruction::print(result_));
    }

    void visit(const Program& node) override {
        for (const StmtPtr& statement : node.statements()) {
            statement->accept(*this);
        }
    }

private:
    TacProgram program_;
    Operand result_;       // operand holding the last visited expression's value
    int tempCounter_ = 0;  // last temporary number handed out
};

}  // namespace

TacProgram TacGenerator::generate(const Program& program) {
    Generator generator;
    return generator.run(program);
}
