# AI-Assisted Compiler Optimization Recommendation and Analysis System

An educational compiler for a small mini-language. A traditional rule-based
optimizer is combined with an AI recommendation layer. The AI only *suggests*
optimizations; the compiler *validates* every suggestion before applying it.

## Pipeline

Source -> Lexer -> Parser -> AST -> Semantic Analysis -> TAC
       -> Optimization Analysis -> Rule-Based Optimizer + AI Recommendation
       -> Validation -> Optimized TAC -> LLVM IR -> Execution -> Performance Analysis

## Requirements

C++17 compiler, Flex, Bison 3.x, CMake, LLVM tools (lli, llc), Python 3, Git.

## Build

    ./scripts/build.sh

## Run

    ./build/aicompiler --version

## Secrets

API keys go in a local `.env` file (ignored by Git). Never commit keys.

## Status

- [x] Milestone 1: project structure
