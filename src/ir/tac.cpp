#include "ir/tac.h"

std::string operandToString(const Operand& operand) {
    switch (operand.kind) {
        case OperandKind::NONE:      return "<none>";
        case OperandKind::CONSTANT:  return std::to_string(operand.value);
        case OperandKind::VARIABLE:
        case OperandKind::TEMPORARY: return operand.name;
    }
    return "<?>";  // unreachable
}

const char* opcodeSymbol(Opcode opcode) {
    switch (opcode) {
        case Opcode::ADD: return "+";
        case Opcode::SUB: return "-";
        case Opcode::MUL: return "*";
        case Opcode::DIV: return "/";
        case Opcode::COPY:
        case Opcode::INPUT:
        case Opcode::PRINT: return "";
    }
    return "?";  // unreachable
}

std::string formatInstruction(const Instruction& instruction) {
    switch (instruction.opcode) {
        case Opcode::COPY:
            return operandToString(instruction.dest) + " = " +
                   operandToString(instruction.arg1);
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::DIV:
            return operandToString(instruction.dest) + " = " +
                   operandToString(instruction.arg1) + " " +
                   opcodeSymbol(instruction.opcode) + " " +
                   operandToString(instruction.arg2);
        case Opcode::INPUT:
            return operandToString(instruction.dest) + " = input()";
        case Opcode::PRINT:
            return "print " + operandToString(instruction.arg1);
    }
    return "<?>";  // unreachable
}

std::string formatTac(const TacProgram& program) {
    std::string text;
    for (const Instruction& instruction : program.instructions) {
        text += formatInstruction(instruction) + "\n";
    }
    return text;
}
