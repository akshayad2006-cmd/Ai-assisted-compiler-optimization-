// file_utils.h - small helpers for reading source files.
#pragma once

#include <optional>
#include <string>

// Reads the whole file at 'path' into a string.
// Returns std::nullopt if the file cannot be opened.
std::optional<std::string> readFile(const std::string& path);
