#pragma once
#include <string>
#include "token.h"

// Simple lexer for the tiny oong language
class Lexer {
public:
  explicit Lexer(const std::string &src) : Src(src), Pos(0) {}
  Token nextToken();

private:
  const std::string Src;
  size_t Pos;
  void skipWhitespace();
  bool skipSingleLineComment();
  bool skipMultiLineComment();
  bool skipHashBang();
  Token makeToken(TokenKind k, size_t start, size_t len, std::optional<int64_t> intVal = std::nullopt) const;
};
