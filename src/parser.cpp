#include "parser.h"

ParseResult Parser::parse() {
  // Expect: print ( INTEGER ) EOF
  if (Cur.kind == TokenKind::Tok_Print) {
    advance();
    if (Cur.kind != TokenKind::Tok_LParen) return error("expected '('");
    advance();
  if (Cur.kind != TokenKind::Tok_Integer) return error("expected integer");
  if (!Cur.intValue.has_value()) return error("invalid integer token");
  int val = static_cast<int>(*Cur.intValue);
    advance();
    if (Cur.kind != TokenKind::Tok_RParen) return error("expected ')'");
    advance();
    if (Cur.kind != TokenKind::Tok_EOF) return error("unexpected tokens after statement");
    return {true, {}, std::make_unique<PrintStmt>(val)};
  }
  return error("expected 'print'");
}
