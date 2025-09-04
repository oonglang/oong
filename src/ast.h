#pragma once
#include <memory>
#include <string>
#include <vector>

// Tiny AST for oong: only PrintStmt(int)
struct Stmt {
  virtual ~Stmt() = default;
};

struct PrintStmt : Stmt {
  int Value;
  explicit PrintStmt(int v) : Value(v) {}
};

// Minimal Type AST for lightweight printing and future wiring
struct Type {
  virtual ~Type() = default;
};

struct NamedType : Type {
  std::string Name;
  NamedType(std::string n) : Name(std::move(n)) {}
};

struct GenericType : Type {
  std::unique_ptr<Type> Base;
  std::vector<std::unique_ptr<Type>> Args;
};

struct ArrayType : Type {
  std::unique_ptr<Type> Element;
  explicit ArrayType(std::unique_ptr<Type> e) : Element(std::move(e)) {}
};

struct UnionType : Type {
  std::vector<std::unique_ptr<Type>> Options;
};

struct IntersectionType : Type {
  std::vector<std::unique_ptr<Type>> Parts;
};

// RawType: conservative sink for object/tuple shapes or other complex forms we
// don't model precisely; stores the raw textual contents consumed by the parser.
struct RawType : Type {
  std::string Raw;
  explicit RawType(std::string r) : Raw(std::move(r)) {}
};

// Utilities
std::string stmtToString(const Stmt* s);
std::string typeToString(const Type* t);
