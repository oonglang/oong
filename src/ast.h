
#pragma once
#include "token.h"
#include <memory>
#include <string>
#include <vector>
#include <utility>


// AST base classes
struct Expr {
  virtual ~Expr() = default;
};

struct Stmt {
  virtual ~Stmt() = default;
};

// Program node: holds a list of statements
struct Program : Stmt {
  std::vector<std::unique_ptr<Stmt>> statements;
  explicit Program(std::vector<std::unique_ptr<Stmt>> stmts)
    : statements(std::move(stmts)) {}
};

// Expression types
struct LiteralExpr : Expr {
  std::string value;
  explicit LiteralExpr(const std::string& v) : value(v) {}
};

struct CallExpr : Expr {
  std::string callee;
  std::vector<std::unique_ptr<Expr>> args;
  CallExpr(const std::string& c, std::vector<std::unique_ptr<Expr>> a)
    : callee(c), args(std::move(a)) {}
};

// Print statement now stores an Expr
struct PrintStmt : Stmt {
  std::unique_ptr<Expr> expr;
    TokenKind origin;
    PrintStmt(std::unique_ptr<Expr> e, TokenKind k)
      : expr(std::move(e)), origin(k) {}
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
