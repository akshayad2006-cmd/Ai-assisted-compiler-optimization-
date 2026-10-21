// tac_parser.h - reads Three Address Code from text.
//
// This lets us test optimizer passes on hand-written TAC, including programs
// the front end can never produce (for example a variable that is reassigned
// between two uses). The text format is exactly what formatTac() prints:
//
//   x = 10            copy
//   t1 = a + b        binary operation  (+  -  *  /)
//   t2 = input()      read an integer
//   print t1          print a value
//
// A name of the form t<digits> is a temporary; any other name is a variable.
// Blank lines and lines starting with // are ignored.
#pragma once

#include <string>
#include <vector>

#include "common/diagnostic.h"
#include "ir/tac.h"

struct TacParseResult {
    TacProgram program;
    std::vector<Diagnostic> errors;  // line = 1-based line number, column = 1
    bool ok() const { return errors.empty(); }
};

// Never throws. Every bad line is reported; good lines are still parsed.
TacParseResult parseTac(const std::string& text);
