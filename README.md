oong — minimal LLVM-backed language

This scaffold builds a tiny 'compiler' that emits LLVM IR which prints a number.

Requirements
- LLVM installed with CMake config files visible to CMake (on Windows, install LLVM and set LLVM_DIR to the lib/cmake/llvm directory).
- CMake 3.13+

Build

Open PowerShell in the project root and run:

```powershell
mkdir build; cd build
cmake .. -G "NMake Makefiles" -DLLVM_DIR="C:/Program Files/LLVM/lib/cmake/llvm"
cmake --build . --config Release
```

Run

From the build directory, run:

```powershell
./oong.exe
```

Next steps
- Add a lexer/parser and lower to LLVM IR.
- Add tests and packaging.
