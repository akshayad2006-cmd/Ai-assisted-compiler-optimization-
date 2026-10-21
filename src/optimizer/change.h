// change.h - the record of one rewrite made by the optimizer.
#pragma once

#include <cstddef>
#include <string>

struct Change {
    std::string pass;    // e.g. "constant_folding"
    std::size_t index;   // position of the instruction when the pass ran
    std::string before;  // instruction text before, e.g. "t1 = 10 * 20"
    std::string after;   // instruction text after,  e.g. "t1 = 200"
};
