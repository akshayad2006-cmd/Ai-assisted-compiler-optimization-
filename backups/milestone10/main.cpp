// main.cpp - entry point of the AI-Assisted Compiler Optimizer.
// Current features: --version, --help, --tokens, --parse, --ast,
// --check, --symbols, --tac, --optimize, --opt-report, --opt-tac,
// --analyze, --analyze-tac.

#include <iostream>
#include <string>
#include <vector>

#include "analysis/analyzer.h"
#include "ast/ast_printer.h"
#include "common/file_utils.h"
#include "ir/tac_generator.h"
#include "ir/tac_parser.h"
#include "lexer/lexer.h"
#include "optimizer/optimizer.h"
#include "parser/parser.h"
#include "semantic/semantic_analyzer.h"
#include "symbol_table/symbol_table.h"
#include "validator/validator.h"

static void printUsage() {
    std::cout << "AI-Assisted Compiler Optimizer\n"
              << "Usage:\n"
              << "  aicompiler --version          Show the version\n"
              << "  aicompiler --help             Show this help\n"
              << "  aicompiler --tokens <file>    Print the tokens of a source file\n"
              << "  aicompiler --parse <file>     Parse a source file (syntax check)\n"
              << "  aicompiler --ast <file>       Print the abstract syntax tree\n"
              << "  aicompiler --check <file>     Run semantic analysis\n"
              << "  aicompiler --symbols <file>   Print the symbol table\n"
              << "  aicompiler --tac <file>       Print the Three Address Code\n"
              << "  aicompiler --optimize <file>  Print the optimized Three Address Code\n"
              << "  aicompiler --opt-report <file> Print every optimization applied\n"
              << "  aicompiler --opt-tac <file>   Optimize hand-written TAC text\n"
              << "  aicompiler --analyze <file>   List optimization candidates (JSON)\n"
              << "  aicompiler --analyze-tac <file> Same, for hand-written TAC text\n"
              << "  aicompiler --validate <file> [recs]     Validate recommendations on source file\n"
              << "  aicompiler --validate-tac <file> [recs] Validate recommendations on TAC file\n";
}

// How far the pipeline runs before printing its result.
enum class Stage { PARSE, AST, CHECK, SYMBOLS, TAC, OPTIMIZE, OPT_REPORT, ANALYZE };

// Prints errors to stderr as "file:line:col: <kind>message".
static void printErrors(const std::string& path, const std::vector<Diagnostic>& errors,
                        const std::string& kind) {
    for (const Diagnostic& error : errors) {
        std::cerr << path << ":" << error.line << ":" << error.column << ": "
                  << kind << error.message << "\n";
    }
}

// Lexes the file and prints one token per line as "[line:column] TOKEN".
static int runTokens(const std::string& path) {
    const auto source = readFile(path);
    if (!source) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return 1;
    }

    const LexResult lexed = Lexer::tokenize(*source);
    for (const Token& token : lexed.tokens) {
        std::cout << "[" << token.line << ":" << token.column << "] "
                  << tokenToString(token) << "\n";
    }
    printErrors(path, lexed.errors, "lexical error: ");
    return lexed.ok() ? 0 : 1;
}

// Reads Three Address Code from a text file, optimizes it and prints the result.
// Used to test optimizer passes on TAC the front end can never produce.
static int runOptTac(const std::string& path) {
    const auto text = readFile(path);
    if (!text) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return 1;
    }
    const TacParseResult parsed = parseTac(*text);
    if (!parsed.ok()) {
        printErrors(path, parsed.errors, "tac error: ");
        return 1;
    }
    std::cout << formatTac(Optimizer::optimize(parsed.program).program);
    return 0;
}

// Reads Three Address Code from a text file and prints its optimization
// candidates as JSON.
static int runAnalyzeTac(const std::string& path) {
    const auto text = readFile(path);
    if (!text) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return 1;
    }
    const TacParseResult parsed = parseTac(*text);
    if (!parsed.ok()) {
        printErrors(path, parsed.errors, "tac error: ");
        return 1;
    }
    std::cout << formatAnalysisJson(analyze(parsed.program));
    return 0;
}

// Finds the recommendations JSON text, either from argv or companion file.
static std::optional<std::string> resolveRecommendations(const std::string& sourcePath,
                                                         int argc, char* argv[], int argIndex) {
    if (argIndex < argc) {
        const std::string recArg = argv[argIndex];
        if (!recArg.empty() && (recArg.front() == '{' || recArg.front() == '[')) {
            return recArg;
        }
        const auto content = readFile(recArg);
        if (!content) {
            std::cerr << "error: cannot open recommendations file '" << recArg << "'\n";
            return std::nullopt;
        }
        return *content;
    }

    std::vector<std::string> candidates = {
        sourcePath + ".recs.json",
        sourcePath + ".recs"
    };
    const auto dot = sourcePath.rfind('.');
    if (dot != std::string::npos) {
        candidates.insert(candidates.begin(), sourcePath.substr(0, dot) + ".recs.json");
        candidates.insert(candidates.begin() + 1, sourcePath.substr(0, dot) + ".recs");
    }

    for (const auto& candidate : candidates) {
        const auto content = readFile(candidate);
        if (content) {
            return *content;
        }
    }

    std::cerr << "error: no recommendations specified and no companion file found for '"
              << sourcePath << "'\n";
    return std::nullopt;
}

static int runValidateTac(const std::string& path, const std::string& recsJson) {
    const auto text = readFile(path);
    if (!text) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return 1;
    }
    const TacParseResult parsed = parseTac(*text);
    if (!parsed.ok()) {
        printErrors(path, parsed.errors, "tac error: ");
        return 1;
    }

    const ValidationReport report = Validator::validateJson(parsed.program, recsJson);
    std::cout << Validator::formatReportJson(report);

    if (report.status != "ok" || report.rejectedCount > 0) {
        if (!report.failureMessage.empty()) {
            std::cerr << "validation error: " << report.failureMessage << "\n";
        }
        for (const auto& dec : report.decisions) {
            if (dec.status == "rejected") {
                std::cerr << "validation error: " << dec.candidateId << ": " << dec.reason << "\n";
            }
        }
        return 1;
    }
    return 0;
}

static int runValidate(const std::string& path, const std::string& recsJson) {
    const auto source = readFile(path);
    if (!source) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return 1;
    }

    const LexResult lexed = Lexer::tokenize(*source);
    if (!lexed.ok()) {
        printErrors(path, lexed.errors, "lexical error: ");
        return 1;
    }

    const ParseResult parsed = Parser::parse(lexed.tokens);
    if (!parsed.ok()) {
        printErrors(path, parsed.errors, "");
        return 1;
    }

    const SemanticResult checked = SemanticAnalyzer::analyze(*parsed.program);
    if (!checked.ok()) {
        printErrors(path, checked.errors, "semantic error: ");
        return 1;
    }

    const TacProgram tac = TacGenerator::generate(*parsed.program);
    const ValidationReport report = Validator::validateJson(tac, recsJson);
    std::cout << Validator::formatReportJson(report);

    if (report.status != "ok" || report.rejectedCount > 0) {
        if (!report.failureMessage.empty()) {
            std::cerr << "validation error: " << report.failureMessage << "\n";
        }
        for (const auto& dec : report.decisions) {
            if (dec.status == "rejected") {
                std::cerr << "validation error: " << dec.candidateId << ": " << dec.reason << "\n";
            }
        }
        return 1;
    }
    return 0;
}

// Runs lexer -> parser -> semantic analysis -> TAC -> optimizer and prints
// the result of the requested stage. A failing stage stops everything after it.
static int runPipeline(const std::string& path, Stage stage) {
    const auto source = readFile(path);
    if (!source) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return 1;
    }

    // Stage 1: lexical analysis.
    const LexResult lexed = Lexer::tokenize(*source);
    if (!lexed.ok()) {
        printErrors(path, lexed.errors, "lexical error: ");
        return 1;
    }

    // Stage 2: syntax analysis (builds the AST).
    const ParseResult parsed = Parser::parse(lexed.tokens);
    if (!parsed.ok()) {
        printErrors(path, parsed.errors, "");
        return 1;
    }
    if (stage == Stage::PARSE) {
        std::cout << "parse OK: " << parsed.program->statements().size()
                  << " statement(s)\n";
        return 0;
    }
    if (stage == Stage::AST) {
        std::cout << printAst(*parsed.program);
        return 0;
    }

    // Stage 3: semantic analysis (builds the symbol table).
    const SemanticResult checked = SemanticAnalyzer::analyze(*parsed.program);
    if (!checked.ok()) {
        printErrors(path, checked.errors, "semantic error: ");
        return 1;
    }
    if (stage == Stage::CHECK) {
        std::cout << "semantic OK: " << parsed.program->statements().size()
                  << " statement(s)\n";
        return 0;
    }
    if (stage == Stage::SYMBOLS) {
        std::cout << formatSymbolTable(checked.symbols);
        return 0;
    }

    // Stage 4: Three Address Code generation.
    const TacProgram tac = TacGenerator::generate(*parsed.program);
    if (stage == Stage::TAC) {
        std::cout << formatTac(tac);
        return 0;
    }

    // Stage 5a: optimization analysis (what COULD be optimized).
    if (stage == Stage::ANALYZE) {
        std::cout << formatAnalysisJson(analyze(tac));
        return 0;
    }

    // Stage 5b: rule-based optimization.
    const OptimizeResult optimized = Optimizer::optimize(tac);
    if (stage == Stage::OPTIMIZE) {
        std::cout << formatTac(optimized.program);
    } else {
        std::cout << formatChanges(optimized);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    const std::string arg = argv[1];

    if (arg == "--version") {
        std::cout << "aicompiler version " << AICOMPILER_VERSION << "\n";
        return 0;
    }
    if (arg == "--help") {
        printUsage();
        return 0;
    }

    const bool isFileCommand = arg == "--tokens" || arg == "--parse" ||
                               arg == "--ast" || arg == "--check" ||
                               arg == "--symbols" || arg == "--tac" ||
                               arg == "--optimize" || arg == "--opt-report" ||
                               arg == "--opt-tac" ||
                               arg == "--analyze" || arg == "--analyze-tac" ||
                               arg == "--validate" || arg == "--validate-tac";
    if (isFileCommand) {
        if (argc < 3) {
            std::cerr << "error: " << arg << " needs a file name\n";
            return 1;
        }
        const std::string path = argv[2];
        if (arg == "--validate" || arg == "--validate-tac") {
            const auto recs = resolveRecommendations(path, argc, argv, 3);
            if (!recs) {
                return 1;
            }
            if (arg == "--validate") return runValidate(path, *recs);
            return runValidateTac(path, *recs);
        }
        if (arg == "--tokens")     return runTokens(path);
        if (arg == "--opt-tac")    return runOptTac(path);
        if (arg == "--analyze-tac") return runAnalyzeTac(path);
        if (arg == "--analyze")    return runPipeline(path, Stage::ANALYZE);
        if (arg == "--parse")      return runPipeline(path, Stage::PARSE);
        if (arg == "--ast")        return runPipeline(path, Stage::AST);
        if (arg == "--check")      return runPipeline(path, Stage::CHECK);
        if (arg == "--symbols")    return runPipeline(path, Stage::SYMBOLS);
        if (arg == "--tac")        return runPipeline(path, Stage::TAC);
        if (arg == "--optimize")   return runPipeline(path, Stage::OPTIMIZE);
        return runPipeline(path, Stage::OPT_REPORT);
    }

    std::cerr << "error: unknown option '" << arg << "'\n";
    printUsage();
    return 1;
}
