// tac.h - Three Address Code (TAC): the compiler's intermediate representation.
//
// TAC is a flat list of simple instructions. Each instruction performs at
// most ONE operation and has at most three "addresses": a destination and
// two inputs. Instructions are stored as structured data (not strings) so
// that the optimizer can inspect operands directly.
#pragma once

#include <string>
#include <utility>
#include <vector>

// What an operand is: nothing (unused slot), a literal number, a variable
// from the source program, or a compiler-generated temporary (t1, t2, ...).
enum class OperandKind { NONE, CONSTANT, VARIABLE, TEMPORARY };

struct Operand {
    OperandKind kind = OperandKind::NONE;
    int value = 0;     // used only when kind == CONSTANT
    std::string name;  // used when kind == VARIABLE or TEMPORARY

    static Operand none() { return Operand{}; }

    static Operand constant(int value) {
        Operand operand;
        operand.kind = OperandKind::CONSTANT;
        operand.value = value;
        return operand;
    }

    static Operand variable(std::string name) {
        Operand operand;
        operand.kind = OperandKind::VARIABLE;
        operand.name = std::move(name);
        return operand;
    }

    // Temporary number 'id' is named "t<id>", e.g. t1.
    static Operand temporary(int id) {
        Operand operand;
        operand.kind = OperandKind::TEMPORARY;
        operand.name = "t" + std::to_string(id);
        return operand;
    }

    bool isNone() const { return kind == OperandKind::NONE; }
    bool isConstant() const { return kind == OperandKind::CONSTANT; }
    bool isVariable() const { return kind == OperandKind::VARIABLE; }
    bool isTemporary() const { return kind == OperandKind::TEMPORARY; }

    bool operator==(const Operand& other) const {
        return kind == other.kind && value == other.value && name == other.name;
    }
    bool operator!=(const Operand& other) const { return !(*this == other); }
};

// The operations an instruction can perform.
enum class Opcode {
    COPY,   // dest = arg1
    ADD,    // dest = arg1 + arg2
    SUB,    // dest = arg1 - arg2
    MUL,    // dest = arg1 * arg2
    DIV,    // dest = arg1 / arg2
    INPUT,  // dest = input()   (reads the next integer at run time)
    PRINT   // print arg1
};

struct Instruction {
    Opcode opcode = Opcode::COPY;
    Operand dest;  // where the result goes (unused for PRINT)
    Operand arg1;  // first input
    Operand arg2;  // second input (only for ADD/SUB/MUL/DIV)

    static Instruction copy(Operand dest, Operand source) {
        Instruction instruction;
        instruction.opcode = Opcode::COPY;
        instruction.dest = std::move(dest);
        instruction.arg1 = std::move(source);
        return instruction;
    }

    static Instruction binary(Opcode opcode, Operand dest, Operand left, Operand right) {
        Instruction instruction;
        instruction.opcode = opcode;
        instruction.dest = std::move(dest);
        instruction.arg1 = std::move(left);
        instruction.arg2 = std::move(right);
        return instruction;
    }

    static Instruction input(Operand dest) {
        Instruction instruction;
        instruction.opcode = Opcode::INPUT;
        instruction.dest = std::move(dest);
        return instruction;
    }

    static Instruction print(Operand value) {
        Instruction instruction;
        instruction.opcode = Opcode::PRINT;
        instruction.arg1 = std::move(value);
        return instruction;
    }

    bool isBinary() const {
        return opcode == Opcode::ADD || opcode == Opcode::SUB ||
               opcode == Opcode::MUL || opcode == Opcode::DIV;
    }
};

// A whole program in TAC form: instructions in execution order.
struct TacProgram {
    std::vector<Instruction> instructions;
};

// Operand as text: "10", "x", "t1".
std::string operandToString(const Operand& operand);

// Operator symbol of a binary opcode: "+", "-", "*", "/".
const char* opcodeSymbol(Opcode opcode);

// One instruction as text: "t1 = 20 * 3", "x = t2", "print y".
std::string formatInstruction(const Instruction& instruction);

// The whole program, one instruction per line (each ending with '\n').
std::string formatTac(const TacProgram& program);
