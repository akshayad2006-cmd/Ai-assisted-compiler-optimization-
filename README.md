# AI-Assisted Compiler Optimization Recommendation and Analysis System

An educational compiler for a small programming language that combines traditional rule-based optimization with an AI-style recommendation layer. The system analyzes source code, suggests additional optimizations, validates recommendations, generates LLVM IR, and supports performance analysis.

## Project Overview

Traditional compiler optimizers apply predefined rules to improve code. This project extends that process with an optimization recommendation pipeline. Recommendations are validated before they can be accepted, helping prevent unsafe transformations.

**Important:** The current recommendation pipeline uses simulated providers for testing; it does not yet connect to a live LLM.

## Architecture

```text
Source Code
    |
    v
Lexer -> Parser -> AST
    |
    v
Semantic Analysis -> Symbol Table
    |
    v
Three-Address Code (TAC)
    |
    v
Rule-Based Optimizer
    |
    v
Optimization Analysis
    |
    v
Python Recommendation Pipeline
    |
    v
C++ Validator
    |
    v
Validated TAC -> LLVM IR
    |
    v
Execution and Performance Analysis
```

The project also includes a React frontend and FastAPI backend for interacting with the compiler pipeline.

## Features

- Lexical analysis and parsing
- Abstract Syntax Tree (AST) generation
- Semantic analysis and symbol table management
- Three-address code (TAC) generation
- Constant folding and constant propagation
- Algebraic simplification and copy propagation
- Common subexpression elimination
- Dead-code elimination
- Python-based optimization recommendations
- C++ recommendation validation
- LLVM IR generation and verification
- Runtime benchmarking and performance analysis
- React, Vite, and Tailwind CSS frontend
- FastAPI backend
- Automated test scripts

## Technology Stack

- **Languages:** C++ and Python
- **Compiler tools:** Flex, Bison, CMake
- **Code generation:** LLVM
- **Backend:** FastAPI
- **Frontend:** React, Vite, Tailwind CSS
- **Testing:** Shell-based test scripts and Python tests

## Prerequisites

Install the following tools before building:

- A C++17-compatible compiler
- CMake
- Flex
- Bison 3.x
- Python 3
- Node.js and npm
- LLVM tools, including `llvm-as`, `opt`, and `lli`

Exact installation commands may vary by operating system.

## Build the Compiler

Clone the repository:

```bash
git clone https://github.com/akshayad2006-cmd/Ai-assisted-compiler-optimization-.git
cd Ai-assisted-compiler-optimization-
```

Build the C++ compiler:

```bash
bash scripts/build.sh
```

## Run the Compiler

Check the compiler version:

```bash
./build/aicompiler --version
```

Run an example through the optimizer:

```bash
./build/aicompiler --optimize examples/demo1.src
```

Generate LLVM IR:

```bash
./build/aicompiler --llvm examples/demo1.src
```

Run a Python recommendation experiment:

```bash
python3 python/optimize_with_ai.py \
  --source examples/demo2.src \
  --compiler build/aicompiler \
  --provider fake:greedy
```

The recommendation command uses a simulated provider rather than a live LLM.

## Run the Web Application

### 1. Start the backend

From the project root:

```bash
python3 -m uvicorn webapp.backend.app:app \
  --reload --host 127.0.0.1 --port 8000
```

### 2. Start the frontend

Open a second terminal:

```bash
cd webapp/frontend
npm install
npm run dev
```

Open the local URL printed by Vite, normally:

http://localhost:5173

The backend health endpoint is normally:

http://127.0.0.1:8000/api/health

Keep both terminals running while using the application.

## Run Tests

Run these commands from the project root:

```bash
bash scripts/run_integration_tests.sh
bash scripts/run_validator_tests.sh
bash scripts/run_python_tests.sh
bash scripts/run_performance_tests.sh
```

Other test scripts are available in the `scripts/` directory.

## Limitations

- The recommendation layer currently uses simulated providers and is not connected to a live LLM.
- Runtime-equivalence checks use sampled test cases and are not a formal proof of correctness.
- Benchmark results depend on the machine, workload, and runtime environment.
- LLVM and other compiler tools must be installed for the corresponding features.

## Future Improvements

- Integrate a real LLM recommendation provider
- Expand test coverage for generated programs and edge cases
- Add more optimization rules and analysis metrics
- Improve cross-platform installation and deployment
- Provide a hosted demonstration of the web application

## Security

Keep API keys, passwords, and other secrets in local environment files. Never commit `.env` files or credentials to the repository.

## Project Status

The compiler pipeline, rule-based optimizations, recommendation testing, validation, LLVM generation, performance analysis, and web interface have been implemented and tested locally.

