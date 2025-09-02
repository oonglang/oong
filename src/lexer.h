#pragma once
#include <string>
#include "token.h"

// Simple lexer for the tiny oong language
class Lexer {
public:
  explicit Lexer(const std::string &src, bool strict = false) : Src(src), Pos(0), StrictMode(strict) {}
  Token nextToken();
  bool IsStrictMode() const { return StrictMode; }
  // Return true if the source contains a line terminator between [from, to)
  bool ContainsLineTerminatorBetween(size_t from, size_t to) const;

private:
  const std::string Src;
  size_t Pos;
  void skipWhitespace();
  bool skipSingleLineComment();
  bool skipHtmlComment();
  bool skipCDataComment();
  bool skipMultiLineComment();
  bool skipHashBang();
  Token makeToken(TokenKind k, size_t start, size_t len, std::optional<int64_t> intVal = std::nullopt) const;
  bool StrictMode{false};
  // Template string state
  bool InTemplateString{false};
  void ProcessTemplateOpenBrace();
  void ProcessTemplateCloseBrace();
  bool IsInTemplateString() const { return InTemplateString; }
  // Heuristic used by the lexer to decide if '/' should start a regex literal
  bool IsRegexPossible() const;
  // Try to scan a RegularExpressionLiteral starting at `start` (initial '/' already consumed).
  // Returns an optional Token: present when we consumed a regex (or an invalid token produced while
  // attempting to scan a regex), or std::nullopt to indicate regex scanning didn't apply (fall back).
  std::optional<Token> scanRegularExpression(size_t start);
  // (public) ContainsLineTerminatorBetween declared above
};

// internal helpers used by lexer core are defined static in lexer.cpp
