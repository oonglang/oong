#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <sstream>

enum class TokenKind {
  Tok_EOF,
  Tok_Print,
  Tok_LParen,
  Tok_RParen,
  Tok_LBrace,
  Tok_RBrace,
  Tok_LBracket,
  Tok_RBracket,
  Tok_Semi,
  Tok_Comma,
  Tok_Assign,
  Tok_Question,
  Tok_QuestionDot,
  Tok_Colon,
  Tok_Ellipsis,
  Tok_Dot,
  Tok_Plus,
  Tok_PlusPlus,
  Tok_Minus,
  Tok_MinusMinus,
  Tok_BitNot,
  Tok_Not,
  Tok_Multiply,
  Tok_Divide,
  Tok_Modulus,
  Tok_Power,
  Tok_NullCoalesce,
  Tok_Hashtag,
  Tok_RightShiftArithmetic,
  Tok_RightShiftArithmeticAssign,
  Tok_RightShiftLogical,
  Tok_RightShiftLogicalAssign,
  Tok_MoreThan,
  Tok_GreaterThanEquals,
  Tok_LeftShiftArithmetic,
  Tok_LeftShiftArithmeticAssign,
  Tok_LessThan,
  Tok_LessThanEquals,
  Tok_Integer,
  Tok_Invalid
};

struct Token {
  TokenKind kind;
  std::string text; // raw text for the token
  size_t pos{0};    // start position in source
  std::optional<int64_t> intValue; // parsed integer value for integer tokens
};

// Debug helper
std::string tokenToString(const Token &t);
