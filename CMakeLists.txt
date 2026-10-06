# Minimum CMake version needed to build this project
cmake_minimum_required(VERSION 3.16)

project(AICompilerOptimizer VERSION 0.1.0 LANGUAGES CXX)

# Use modern C++17 and make it mandatory
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json (helps VS Code understand the code)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Turn on useful warnings so we catch mistakes early
add_compile_options(-Wall -Wextra)

# Our compiler executable. We add more source files here in later milestones.
add_executable(aicompiler
    src/main.cpp
)

# Lets us write #include "common/xyz.h" from anywhere inside src/
target_include_directories(aicompiler PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Pass the project version into the code as a macro
target_compile_definitions(aicompiler PRIVATE AICOMPILER_VERSION="${PROJECT_VERSION}")
