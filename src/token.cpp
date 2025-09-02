#include "token.h"

std::string tokenToString(const Token &t)
{
  std::ostringstream os;
  os << "Token(";
  switch (t.kind)
  {
  case TokenKind::Tok_EOF:
    os << "EOF";
    break;
  case TokenKind::Tok_Print:
    os << "Print";
    break;
  case TokenKind::Tok_Break:
    os << "Break";
    break;
  case TokenKind::Tok_Do:
    os << "Do";
    break;
  case TokenKind::Tok_Instanceof:
    os << "Instanceof";
    break;
  case TokenKind::Tok_Typeof:
    os << "Typeof";
    break;
  case TokenKind::Tok_Case:
    os << "Case";
    break;
  case TokenKind::Tok_Else:
    os << "Else";
    break;
  case TokenKind::Tok_New:
    os << "New";
    break;
  case TokenKind::Tok_Var:
    os << "Var";
    break;
  case TokenKind::Tok_Catch:
    os << "Catch";
    break;
  case TokenKind::Tok_Finally:
    os << "Finally";
    break;
  case TokenKind::Tok_Void:
    os << "Void";
    break;
  case TokenKind::Tok_Continue:
    os << "Continue";
    break;
  case TokenKind::Tok_For:
    os << "For";
    break;
  case TokenKind::Tok_Switch:
    os << "Switch";
    break;
  case TokenKind::Tok_While:
    os << "While";
    break;
  case TokenKind::Tok_Debugger:
    os << "Debugger";
    break;
  case TokenKind::Tok_Function:
    os << "Function";
    break;
  case TokenKind::Tok_This:
    os << "This";
    break;
  case TokenKind::Tok_With:
    os << "With";
    break;
  case TokenKind::Tok_Default:
    os << "Default";
    break;
  case TokenKind::Tok_If:
    os << "If";
    break;
  case TokenKind::Tok_Throw:
    os << "Throw";
    break;
  case TokenKind::Tok_Delete:
    os << "Delete";
    break;
  case TokenKind::Tok_In:
    os << "In";
    break;
  case TokenKind::Tok_Try:
    os << "Try";
    break;
  case TokenKind::Tok_As:
    os << "As";
    break;
  case TokenKind::Tok_From:
    os << "From";
    break;
  case TokenKind::Tok_Of:
    os << "Of";
    break;
  case TokenKind::Tok_Yield:
    os << "Yield";
    break;
  case TokenKind::Tok_YieldStar:
    os << "YieldStar";
    break;
  case TokenKind::Tok_Class:
    os << "Class";
    break;
  case TokenKind::Tok_Enum:
    os << "Enum";
    break;
  case TokenKind::Tok_Extends:
    os << "Extends";
    break;
  case TokenKind::Tok_Super:
    os << "Super";
    break;
  case TokenKind::Tok_Const:
    os << "Const";
    break;
  case TokenKind::Tok_Export:
    os << "Export";
    break;
  case TokenKind::Tok_Import:
    os << "Import";
    break;
  case TokenKind::Tok_Async:
    os << "Async";
    break;
  case TokenKind::Tok_Await:
    os << "Await";
    break;
  case TokenKind::Tok_Implements:
    os << "Implements";
    break;
  case TokenKind::Tok_Private:
    os << "Private";
    break;
  case TokenKind::Tok_Public:
    os << "Public";
    break;
  case TokenKind::Tok_Interface:
    os << "Interface";
    break;
  case TokenKind::Tok_Package:
    os << "Package";
    break;
  case TokenKind::Tok_Protected:
    os << "Protected";
    break;
  case TokenKind::Tok_Static:
    os << "Static";
    break;
  case TokenKind::Tok_StrictLet:
    os << "StrictLet";
    break;
  case TokenKind::Tok_NonStrictLet:
    os << "NonStrictLet";
    break;
  case TokenKind::Tok_Return:
    os << "Return";
    break;
  case TokenKind::Tok_LParen:
    os << "LParen";
    break;
  case TokenKind::Tok_RParen:
    os << "RParen";
    break;
  case TokenKind::Tok_LBracket:
    os << "LBracket";
    break;
  case TokenKind::Tok_RBracket:
    os << "RBracket";
    break;
  case TokenKind::Tok_LBrace:
    os << "LBrace";
    break;
  case TokenKind::Tok_RBrace:
    os << "RBrace";
    break;
  case TokenKind::Tok_Semi:
    os << "Semi";
    break;
  case TokenKind::Tok_Comma:
    os << "Comma";
    break;
  case TokenKind::Tok_Assign:
    os << "Assign";
    break;
  case TokenKind::Tok_Arrow:
    os << "Arrow";
    break;
  case TokenKind::Tok_Question:
    os << "Question";
    break;
  case TokenKind::Tok_QuestionDot:
    os << "QuestionDot";
    break;
  case TokenKind::Tok_Colon:
    os << "Colon";
    break;
  case TokenKind::Tok_Ellipsis:
    os << "Ellipsis";
    break;
  case TokenKind::Tok_Dot:
    os << "Dot";
    break;
  case TokenKind::Tok_Plus:
    os << "Plus";
    break;
  case TokenKind::Tok_PlusPlus:
    os << "PlusPlus";
    break;
  case TokenKind::Tok_PlusAssign:
    os << "PlusAssign";
    break;
  case TokenKind::Tok_Minus:
    os << "Minus";
    break;
  case TokenKind::Tok_MinusMinus:
    os << "MinusMinus";
    break;
  case TokenKind::Tok_MinusAssign:
    os << "MinusAssign";
    break;
  case TokenKind::Tok_BitNot:
    os << "BitNot";
    break;
  case TokenKind::Tok_Not:
    os << "Not";
    break;
  case TokenKind::Tok_Multiply:
    os << "Multiply";
    break;
  case TokenKind::Tok_Divide:
    os << "Divide";
    break;
  case TokenKind::Tok_Modulus:
    os << "Modulus";
    break;
  case TokenKind::Tok_Power:
    os << "Power";
    break;
  case TokenKind::Tok_ModulusAssign:
    os << "ModulusAssign";
    break;
  case TokenKind::Tok_PowerAssign:
    os << "PowerAssign";
    break;
  case TokenKind::Tok_NullishCoalescingAssign:
    os << "NullishCoalescingAssign";
    break;
  case TokenKind::Tok_MultiplyAssign:
    os << "MultiplyAssign";
    break;
  case TokenKind::Tok_NullCoalesce:
    os << "NullCoalesce";
    break;
  case TokenKind::Tok_Hashtag:
    os << "Hashtag";
    break;
  case TokenKind::Tok_RightShiftArithmetic:
    os << "RightShiftArithmetic";
    break;
  case TokenKind::Tok_RightShiftArithmeticAssign:
    os << "RightShiftArithmeticAssign";
    break;
  case TokenKind::Tok_RightShiftLogical:
    os << "RightShiftLogical";
    break;
  case TokenKind::Tok_RightShiftLogicalAssign:
    os << "RightShiftLogicalAssign";
    break;
  case TokenKind::Tok_MoreThan:
    os << "MoreThan";
    break;
  case TokenKind::Tok_GreaterThanEquals:
    os << "GreaterThanEquals";
    break;
  case TokenKind::Tok_LeftShiftArithmetic:
    os << "LeftShiftArithmetic";
    break;
  case TokenKind::Tok_LeftShiftArithmeticAssign:
    os << "LeftShiftArithmeticAssign";
    break;
  case TokenKind::Tok_LessThan:
    os << "LessThan";
    break;
  case TokenKind::Tok_LessThanEquals:
    os << "LessThanEquals";
    break;
  case TokenKind::Tok_Equals:
    os << "Equals";
    break;
  case TokenKind::Tok_IdentityEquals:
    os << "IdentityEquals";
    break;
  case TokenKind::Tok_NotEquals:
    os << "NotEquals";
    break;
  case TokenKind::Tok_IdentityNotEquals:
    os << "IdentityNotEquals";
    break;
  case TokenKind::Tok_BitAnd:
    os << "BitAnd";
    break;
  case TokenKind::Tok_LogicalAnd:
    os << "LogicalAnd";
    break;
  case TokenKind::Tok_BitAndAssign:
    os << "BitAndAssign";
    break;
  case TokenKind::Tok_BitXor:
    os << "BitXor";
    break;
  case TokenKind::Tok_BitXorAssign:
    os << "BitXorAssign";
    break;
  case TokenKind::Tok_BitOr:
    os << "BitOr";
    break;
  case TokenKind::Tok_LogicalOr:
    os << "LogicalOr";
    break;
  case TokenKind::Tok_BitOrAssign:
    os << "BitOrAssign";
    break;
  case TokenKind::Tok_DivideAssign:
    os << "DivideAssign";
    break;
  case TokenKind::Tok_NullLiteral:
    os << "NullLiteral";
    break;
  case TokenKind::Tok_BooleanLiteral:
    os << "BooleanLiteral";
    break;
  case TokenKind::Tok_BackTick:
    os << "BackTick";
    break;
  case TokenKind::Tok_TemplateStringAtom:
    os << "TemplateStringAtom";
    break;
  case TokenKind::Tok_TemplateStringStartExpression:
    os << "TemplateStringStartExpression";
    break;
  case TokenKind::Tok_TemplateCloseBrace:
    os << "TemplateCloseBrace";
    break;
  case TokenKind::Tok_StringLiteral:
    os << "StringLiteral";
    break;
  case TokenKind::Tok_DecimalLiteral:
    os << "DecimalLiteral";
    break;
  case TokenKind::Tok_BigDecimalIntegerLiteral:
    os << "BigDecimalIntegerLiteral";
    break;
  case TokenKind::Tok_HexIntegerLiteral:
    os << "HexIntegerLiteral";
    break;
  case TokenKind::Tok_BigHexIntegerLiteral:
    os << "BigHexIntegerLiteral";
    break;
  case TokenKind::Tok_OctalIntegerLiteral:
    os << "OctalIntegerLiteral";
    break;
  case TokenKind::Tok_OctalIntegerLiteral2:
    os << "OctalIntegerLiteral2";
    break;
  case TokenKind::Tok_BigOctalIntegerLiteral:
    os << "BigOctalIntegerLiteral";
    break;
  case TokenKind::Tok_BinaryIntegerLiteral:
    os << "BinaryIntegerLiteral";
    break;
  case TokenKind::Tok_BigBinaryIntegerLiteral:
    os << "BigBinaryIntegerLiteral";
    break;
  case TokenKind::Tok_Integer:
    os << "Integer(" << (t.intValue ? std::to_string(*t.intValue) : t.text) << ")";
    break;
  case TokenKind::Tok_Invalid:
    os << "Invalid(" << t.text << ")";
    break;
  }
  os << ", pos=" << t.pos << ")";
  return os.str();
}
