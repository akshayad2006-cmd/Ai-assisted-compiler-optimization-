// passes.h - the individual optimization passes.
//
// Two kinds:
//  * INSTRUCTION RULES look at ONE instruction and return a cheaper
//    replacement, or std::nullopt ("does not apply here"). The validator in
//    a later milestone can ask these very same rules "is this legal?".
//  * PROGRAM PASSES need to see the whole program (what is known, what is
//    used later). They modify the program and return what they changed.
#pragma once

#include <optional>
#include <vector>

#include "ir/tac.h"
#include "optimizer/change.h"

// ---------------------------------------------------------------------------
// Instruction rules
// ---------------------------------------------------------------------------

// CONSTANT FOLDING: both operands are constants, so compute at compile time.
//   t1 = 10 * 20   ->   t1 = 200
// Not applied when it would be unsafe: division by zero, or when the result
// does not fit in a 32-bit int (the program's runtime behaviour is kept).
std::optional<Instruction> tryConstantFold(const Instruction& instruction);

// ALGEBRAIC SIMPLIFICATION: identities that remove an operation.
//   x + 0, 0 + x, x - 0, x * 1, 1 * x, x / 1   ->   x
//   x * 0, 0 * x                               ->   0
// Instructions with two constants are left to constant folding.
// Nothing is done for division by zero.
std::optional<Instruction> trySimplifyAlgebra(const Instruction& instruction);

// ---------------------------------------------------------------------------
// Program passes
// ---------------------------------------------------------------------------

// CONSTANT PROPAGATION: replace a variable or temporary whose value is a
// known constant with that constant.
//   x = 10; y = x + 5;   ->   x = 10; y = 10 + 5;
// Walks the (straight-line) program from top to bottom, remembering which
// names currently hold a constant and forgetting them when reassigned.
std::vector<Change> propagateConstants(TacProgram& program);

// COPY PROPAGATION: after "x = y" (a plain copy between two names), later
// uses of x can read y directly, until x or y is assigned again.
//   t3 = a; t4 = t3; x = t4;   ->   t3 = a; t4 = a; x = a;
// Also removes self-copies ("x = x"), which do nothing. The copies that become
// unused are then removed by dead code elimination.
std::vector<Change> propagateCopies(TacProgram& program);

// COMMON SUBEXPRESSION ELIMINATION (CSE): if the same operation on the same
// operands was already computed and nothing it depends on changed since,
// reuse the earlier result instead of computing it again.
//   t5 = a + b; t6 = a + b;   ->   t5 = a + b; t6 = t5;
// Never applied to input(): every input() reads a NEW value.
std::vector<Change> eliminateCommonSubexpressions(TacProgram& program);

// DEAD CODE ELIMINATION: remove an instruction whose result is never used.
// Walks the program from bottom to top keeping the set of names that are
// still needed ("live"). 'print' is the only thing that makes a value needed.
// Instructions with side effects are kept even when unused: input() (it
// consumes a value) and a division that could fail at runtime (divisor not
// a known constant other than 0 and -1), so neither a read nor a fault is ever removed.
std::vector<Change> eliminateDeadCode(TacProgram& program);
