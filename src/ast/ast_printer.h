// ast_printer.h - prints an AST as an indented tree, for debugging.
#pragma once

#include <string>

#include "ast/ast.h"

// Returns the tree drawn with box characters, e.g.
//   Program
//   └── Declaration x
//       └── +
//           ├── 10
//           └── 3
std::string printAst(const Program& program);
