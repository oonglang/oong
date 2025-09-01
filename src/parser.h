#pragma once
#include "lexer.h"
#include <memory>
#include <optional>
#include <string>

#include "ast.h"

// Parser result
struct ParseResult {
  bool ok;
  std::string error;
  std::unique_ptr<Stmt> stmt;
};

class Parser {
public:
  explicit Parser(const std::string &src) : L(src) { Cur = L.nextToken(); }
  ParseResult parse();

private:
  Lexer L;
  Token Cur;
  void advance() { Cur = L.nextToken(); }
  ParseResult error(const std::string &msg) { return {false, msg, nullptr}; }
};
