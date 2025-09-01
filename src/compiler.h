#pragma once
#include <string>

// Compile the source file at inputPath to an executable at outPath.
// Returns 0 on success, non-zero on failure.
int run_compiler(const std::string &inputPath, const std::string &outPath);
