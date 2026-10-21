#include "common/file_utils.h"

#include <fstream>
#include <sstream>

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();  // copy the whole file into the buffer
    return buffer.str();
}
