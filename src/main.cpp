// main.cpp - entry point of the AI-Assisted Compiler Optimizer.
// For now it only handles --version and --help.
// In later milestones it will run the full compilation pipeline.

#include <iostream>
#include <string>

static void printUsage() {
    std::cout << "AI-Assisted Compiler Optimizer\n"
              << "Usage:\n"
              << "  aicompiler --version     Show the version\n"
              << "  aicompiler --help        Show this help\n";
}

int main(int argc, char* argv[]) {
    // No arguments: show usage and exit with an error code (1).
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

    // Anything else is an error. Errors go to std::cerr, not std::cout.
    std::cerr << "error: unknown option '" << arg << "'\n";
    printUsage();
    return 1;
}
