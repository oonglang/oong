#include "token.h"

std::string tokenToString(const Token &t) {
  std::ostringstream os;
  os << "Token(";
  switch (t.kind) {
    case TokenKind::Tok_EOF: os << "EOF"; break;
    case TokenKind::Tok_Print: os << "Print"; break;
    case TokenKind::Tok_LParen: os << "LParen"; break;
    case TokenKind::Tok_RParen: os << "RParen"; break;
    case TokenKind::Tok_LBracket: os << "LBracket"; break;
    case TokenKind::Tok_RBracket: os << "RBracket"; break;
    case TokenKind::Tok_LBrace: os << "LBrace"; break;
    case TokenKind::Tok_RBrace: os << "RBrace"; break;
    case TokenKind::Tok_Semi: os << "Semi"; break;
    case TokenKind::Tok_Comma: os << "Comma"; break;
    case TokenKind::Tok_Assign: os << "Assign"; break;
    case TokenKind::Tok_Question: os << "Question"; break;
    case TokenKind::Tok_QuestionDot: os << "QuestionDot"; break;
    case TokenKind::Tok_Colon: os << "Colon"; break;
    case TokenKind::Tok_Ellipsis: os << "Ellipsis"; break;
    case TokenKind::Tok_Dot: os << "Dot"; break;
  case TokenKind::Tok_Plus: os << "Plus"; break;
  case TokenKind::Tok_PlusPlus: os << "PlusPlus"; break;
  case TokenKind::Tok_Minus: os << "Minus"; break;
  case TokenKind::Tok_MinusMinus: os << "MinusMinus"; break;
  case TokenKind::Tok_BitNot: os << "BitNot"; break;
  case TokenKind::Tok_Not: os << "Not"; break;
  case TokenKind::Tok_Multiply: os << "Multiply"; break;
  case TokenKind::Tok_Divide: os << "Divide"; break;
  case TokenKind::Tok_Modulus: os << "Modulus"; break;
  case TokenKind::Tok_Power: os << "Power"; break;
  case TokenKind::Tok_NullCoalesce: os << "NullCoalesce"; break;
  case TokenKind::Tok_Hashtag: os << "Hashtag"; break;
  case TokenKind::Tok_RightShiftArithmetic: os << "RightShiftArithmetic"; break;
  case TokenKind::Tok_RightShiftArithmeticAssign: os << "RightShiftArithmeticAssign"; break;
  case TokenKind::Tok_RightShiftLogical: os << "RightShiftLogical"; break;
  case TokenKind::Tok_RightShiftLogicalAssign: os << "RightShiftLogicalAssign"; break;
  case TokenKind::Tok_MoreThan: os << "MoreThan"; break;
  case TokenKind::Tok_GreaterThanEquals: os << "GreaterThanEquals"; break;
  case TokenKind::Tok_LeftShiftArithmetic: os << "LeftShiftArithmetic"; break;
  case TokenKind::Tok_LeftShiftArithmeticAssign: os << "LeftShiftArithmeticAssign"; break;
  case TokenKind::Tok_LessThan: os << "LessThan"; break;
  case TokenKind::Tok_LessThanEquals: os << "LessThanEquals"; break;
    case TokenKind::Tok_Integer: os << "Integer(" << (t.intValue ? std::to_string(*t.intValue) : t.text) << ")"; break;
    case TokenKind::Tok_Invalid: os << "Invalid(" << t.text << ")"; break;
  }
  os << ", pos=" << t.pos << ")";
  return os.str();
}
