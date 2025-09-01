#pragma once
#include <memory>
#include <string>

// Tiny AST for oong: only PrintStmt(int)
struct Stmt {
  virtual ~Stmt() = default;
};

struct PrintStmt : Stmt {
  int Value;
  explicit PrintStmt(int v) : Value(v) {}
};

// Utilities
std::string stmtToString(const Stmt* s);
