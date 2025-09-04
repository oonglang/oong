#include <iostream>
#include <fstream>
#include <iterator>
#include <string>

#include "interpreter.h"
#include "compiler.h"

// Simple delegating CLI for oong: run interpreter or compiler
int main(int argc, char **argv) {
    std::string inputPath;
    std::string outPath;
    bool doCompile = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-c" && i + 1 < argc) { doCompile = true; inputPath = argv[++i]; }
        else if (a == "-o" && i + 1 < argc) { outPath = argv[++i]; }
        else if (a == "-h" || a == "--help") { std::cout << "Usage: oong [-c input.oo -o out.exe] [input.oo]\n"; return 0; }
        else if (inputPath.empty()) { inputPath = a; }
    }

    if (inputPath.empty()) { std::cerr << "No input file provided\n"; return 2; }

    std::ifstream in(inputPath);
    if (!in) { std::cerr << "Could not open file: " << inputPath << "\n"; return 2; }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    if (!doCompile) {
        int r = run_interpreter(content);
        if (r == 0) return 0; // interpreter handled it
        else return r; // interpreter failed, return error code
    }

    return run_compiler(inputPath, outPath);
}
