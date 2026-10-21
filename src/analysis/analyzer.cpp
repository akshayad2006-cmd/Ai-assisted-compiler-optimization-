#include "analysis/analyzer.h"

#include <algorithm>
#include <map>
#include <optional>

#include "ir/tac_parser.h"
#include "optimizer/optimizer.h"
#include "optimizer/passes.h"

namespace {

int programCost(const TacProgram& program) {
    int total = 0;
    for (const Instruction& instruction : program.instructions) {
        total += instructionCost(instruction);
    }
    return total;
}

// Cost of an instruction given as text such as "t1 = 30" ("(removed)" = 0).
// Falls back to 'fallback' if the text cannot be read (should not happen).
int costOfText(const std::string& text, int fallback) {
    if (text == "(removed)") {
        return 0;
    }
    const TacParseResult parsed = parseTac(text + "\n");
    if (!parsed.ok() || parsed.program.instructions.size() != 1) {
        return fallback;
    }
    return instructionCost(parsed.program.instructions[0]);
}

using InstructionRule = std::optional<Instruction> (*)(const Instruction&);

// Asks an instruction rule about every instruction of the unchanged program.
void collectFromRule(const char* name, InstructionRule rule, const TacProgram& program,
                     std::vector<Change>& out) {
    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        const std::optional<Instruction> replacement = rule(program.instructions[i]);
        if (replacement) {
            out.push_back(Change{name, i, formatInstruction(program.instructions[i]),
                                 formatInstruction(*replacement)});
        }
    }
}

// Runs a whole-program pass on a COPY and keeps its change log. Every pass
// sees the original program, so the positions in the log are original
// positions.
void collectFromPass(std::vector<Change> (*pass)(TacProgram&), const TacProgram& program,
                     std::vector<Change>& out) {
    TacProgram copy = program;
    const std::vector<Change> changes = pass(copy);
    out.insert(out.end(), changes.begin(), changes.end());
}

std::string quote(const std::string& text) {
    std::string result = "\"";
    for (const char c : text) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    result += "?";  // other control characters never occur in TAC text
                } else {
                    result += c;
                }
        }
    }
    return result + "\"";
}

// "[]" or one element per line, comma separated.
std::string jsonArray(const std::vector<std::string>& elements) {
    if (elements.empty()) {
        return "[]";
    }
    std::string text = "[\n";
    for (std::size_t i = 0; i < elements.size(); ++i) {
        text += "    " + elements[i] + (i + 1 < elements.size() ? ",\n" : "\n");
    }
    return text + "  ]";
}

}  // namespace

int instructionCost(const Instruction& instruction) {
    switch (instruction.opcode) {
        case Opcode::COPY:  return 1;
        case Opcode::ADD:   return 1;
        case Opcode::SUB:   return 1;
        case Opcode::MUL:   return 3;
        case Opcode::DIV:   return 10;
        case Opcode::INPUT: return 5;
        case Opcode::PRINT: return 2;
    }
    return 0;
}

AnalysisReport analyze(const TacProgram& program) {
    AnalysisReport report;
    report.program = program;
    report.staticCost = programCost(program);

    // 1. Ask every rule, each on the unchanged program.
    std::vector<Change> findings;
    collectFromPass(propagateConstants, program, findings);
    collectFromRule("constant_folding", tryConstantFold, program, findings);
    collectFromRule("algebraic_simplification", trySimplifyAlgebra, program, findings);
    collectFromPass(propagateCopies, program, findings);
    collectFromPass(eliminateCommonSubexpressions, program, findings);
    collectFromPass(eliminateDeadCode, program, findings);

    // 2a. One pass can touch the same instruction twice, e.g. copy propagation
    // turns "a = d" into "a = a" and then removes that useless self-copy. The
    // reader only cares about the whole step "a = d -> (removed)", so
    // consecutive findings of one pass on one instruction become one.
    std::vector<Change> merged;
    for (const Change& finding : findings) {
        if (!merged.empty() && merged.back().pass == finding.pass &&
            merged.back().index == finding.index && merged.back().after == finding.before) {
            merged.back().after = finding.after;
        } else {
            merged.push_back(finding);
        }
    }
    findings = merged;

    // 2b. Program order; for the same instruction keep the rule order above.
    std::stable_sort(findings.begin(), findings.end(),
                     [](const Change& a, const Change& b) { return a.index < b.index; });

    std::map<std::size_t, std::string> deadCodeIds;  // 0-based position -> id
    for (std::size_t n = 0; n < findings.size(); ++n) {
        const Change& finding = findings[n];
        Candidate candidate;
        candidate.id = "C" + std::to_string(n + 1);
        candidate.rule = finding.pass;
        candidate.instruction = finding.index + 1;
        candidate.before = finding.before;
        candidate.after = finding.after;
        candidate.removesInstruction = finding.after == "(removed)";
        const int costBefore = instructionCost(program.instructions[finding.index]);
        candidate.costDelta = costOfText(finding.after, costBefore) - costBefore;
        if (finding.pass == "dead_code_elimination") {
            deadCodeIds[finding.index] = candidate.id;
        }
        report.candidates.push_back(candidate);
    }

    // 3. Dead-code removals that depend on each other: instruction k can only
    // go if every instruction that reads its result (before it is written
    // again) goes too.
    for (Candidate& candidate : report.candidates) {
        if (candidate.rule != "dead_code_elimination") {
            continue;
        }
        const std::size_t k = candidate.instruction - 1;
        const Operand& destination = program.instructions[k].dest;
        for (std::size_t j = k + 1; j < program.instructions.size(); ++j) {
            const Instruction& later = program.instructions[j];
            const bool reads = later.arg1 == destination ||
                               (later.isBinary() && later.arg2 == destination);
            if (reads) {
                const auto found = deadCodeIds.find(j);
                if (found != deadCodeIds.end() &&
                    std::find(candidate.requires_.begin(), candidate.requires_.end(),
                              found->second) == candidate.requires_.end()) {
                    candidate.requires_.push_back(found->second);
                }
            }
            if (later.opcode != Opcode::PRINT && later.dest == destination) {
                break;  // overwritten: later readers see a different value
            }
        }
    }

    // 4. Rewrites that look possible but are refused for safety.
    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        const Instruction& instruction = program.instructions[i];
        if (instruction.isBinary() && instruction.arg1.isConstant() &&
            instruction.arg2.isConstant() && !tryConstantFold(instruction)) {
            BlockedRewrite blocked;
            blocked.rule = "constant_folding";
            blocked.instruction = i + 1;
            blocked.text = formatInstruction(instruction);
            blocked.reason = (instruction.opcode == Opcode::DIV && instruction.arg2.value == 0)
                                 ? "division by zero"
                                 : "result does not fit in a 32-bit int";
            report.blocked.push_back(blocked);
        }
    }

    // 5. What the real optimizer achieves overall.
    const OptimizeResult optimized = Optimizer::optimize(program);
    report.optimizedInstructionCount = optimized.program.instructions.size();
    report.optimizedStaticCost = programCost(optimized.program);
    return report;
}

std::string formatAnalysisJson(const AnalysisReport& report) {
    std::vector<std::string> instructions;
    for (std::size_t i = 0; i < report.program.instructions.size(); ++i) {
        instructions.push_back("{\"index\": " + std::to_string(i + 1) + ", \"text\": " +
                               quote(formatInstruction(report.program.instructions[i])) + "}");
    }

    std::vector<std::string> candidates;
    for (const Candidate& c : report.candidates) {
        std::string requires_text = "[";
        for (std::size_t i = 0; i < c.requires_.size(); ++i) {
            requires_text += (i ? ", " : "") + quote(c.requires_[i]);
        }
        requires_text += "]";
        candidates.push_back(
            "{\"id\": " + quote(c.id) + ", \"rule\": " + quote(c.rule) +
            ", \"instruction\": " + std::to_string(c.instruction) +
            ", \"before\": " + quote(c.before) + ", \"after\": " + quote(c.after) +
            ", \"removes_instruction\": " + (c.removesInstruction ? "true" : "false") +
            ", \"static_cost_delta\": " + std::to_string(c.costDelta) +
            ", \"requires\": " + requires_text + "}");
    }

    std::vector<std::string> blocked;
    for (const BlockedRewrite& b : report.blocked) {
        blocked.push_back("{\"rule\": " + quote(b.rule) +
                          ", \"instruction\": " + std::to_string(b.instruction) +
                          ", \"text\": " + quote(b.text) +
                          ", \"reason\": " + quote(b.reason) + "}");
    }

    std::string text = "{\n";
    text += "  \"schema_version\": 1,\n";
    text += "  \"program\": {\n";
    text += "    \"instruction_count\": " + std::to_string(report.program.instructions.size()) + ",\n";
    text += "    \"static_cost\": " + std::to_string(report.staticCost) + ",\n";
    text += "    \"optimized_instruction_count\": " + std::to_string(report.optimizedInstructionCount) + ",\n";
    text += "    \"optimized_static_cost\": " + std::to_string(report.optimizedStaticCost) + "\n";
    text += "  },\n";
    text += "  \"cost_model\": {\"copy\": 1, \"add\": 1, \"sub\": 1, \"mul\": 3, \"div\": 10, "
            "\"input\": 5, \"print\": 2},\n";
    text += "  \"instructions\": " + jsonArray(instructions) + ",\n";
    text += "  \"candidates\": " + jsonArray(candidates) + ",\n";
    text += "  \"blocked\": " + jsonArray(blocked) + "\n";
    text += "}\n";
    return text;
}
