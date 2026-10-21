cmake_minimum_required(VERSION 3.16)

project(AICompilerOptimizer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

find_package(FLEX REQUIRED)
FLEX_TARGET(LexerGen
    src/lexer/lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cc
)

find_package(BISON 3.2 REQUIRED)
BISON_TARGET(ParserGen
    src/parser/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cc
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hh
)

find_package(nlohmann_json REQUIRED)

add_executable(aicompiler
    src/main.cpp
    src/common/file_utils.cpp
    src/lexer/token.cpp
    src/ast/ast_printer.cpp
    src/symbol_table/symbol_table.cpp
    src/semantic/semantic_analyzer.cpp
    src/ir/tac.cpp
    src/ir/tac_generator.cpp
    src/ir/tac_parser.cpp
    src/ir/tac_interpreter.cpp
    src/analysis/analyzer.cpp
    src/optimizer/passes.cpp
    src/optimizer/optimizer.cpp
    src/validator/validator.cpp
    ${FLEX_LexerGen_OUTPUTS}
    ${BISON_ParserGen_OUTPUTS}
)

target_include_directories(aicompiler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(aicompiler PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(aicompiler PRIVATE AICOMPILER_VERSION="${PROJECT_VERSION}")
