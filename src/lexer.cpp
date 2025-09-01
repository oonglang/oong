#include "lexer.h"
#include "token.h"
#include <cctype>
#include <sstream>

// Helper: detect UTF-8 encoded U+2028 (E2 80 A8) and U+2029 (E2 80 A9)
static inline bool isUtf8LineSeparator(const std::string &s, size_t pos) {
  if (pos + 2 >= s.size()) return false;
  unsigned char a = static_cast<unsigned char>(s[pos]);
  unsigned char b = static_cast<unsigned char>(s[pos+1]);
  unsigned char c = static_cast<unsigned char>(s[pos+2]);
  return a == 0xE2 && b == 0x80 && (c == 0xA8 || c == 0xA9);
}

// If there's a line terminator at pos, return its byte length (1 for \r/\n, 3 for UTF-8 separators), otherwise 0
static inline size_t lineTerminatorLength(const std::string &s, size_t pos) {
  if (pos >= s.size()) return 0;
  unsigned char ch = static_cast<unsigned char>(s[pos]);
  if (ch == '\n' || ch == '\r') return 1;
  if (isUtf8LineSeparator(s, pos)) return 3;
  return 0;
}

void Lexer::skipWhitespace() {
  while (Pos < Src.size()) {
    unsigned char c = (unsigned char)Src[Pos];
    if (isspace(c)) {
      ++Pos; // regular ASCII whitespace
      continue;
    }

    // treat Unicode line separators (U+2028/U+2029) same as newline
    size_t ltlen = lineTerminatorLength(Src, Pos);
    if (ltlen) {
      Pos += ltlen;
      continue;
    }

  // try hash-bang (only allowed at start)
  if (skipHashBang()) continue;

  // try comment skipping
  if (skipSingleLineComment()) continue;
  // try multi-line comment
  if (skipMultiLineComment()) continue;

    break; // non-whitespace, non-comment char
  }
}

bool Lexer::skipSingleLineComment() {
  if (Src[Pos] != '/' || Pos + 1 >= Src.size() || Src[Pos + 1] != '/') return false;
  Pos += 2; // skip '//'
  while (Pos < Src.size()) {
    size_t lt = lineTerminatorLength(Src, Pos);
    if (lt) break;
    ++Pos;
  }
  return true;
}

bool Lexer::skipHashBang() {
  // only at start of file
  if (Pos != 0) return false;
  if (Src[Pos] != '#' || Pos + 1 >= Src.size() || Src[Pos + 1] != '!') return false;
  Pos += 2; // skip '#!'
  while (Pos < Src.size()) {
    size_t lt = lineTerminatorLength(Src, Pos);
    if (lt) break;
    ++Pos;
  }
  return true;
}

Token Lexer::makeToken(TokenKind k, size_t start, size_t len, std::optional<int64_t> intVal) const {
  Token t{ k, Src.substr(start, len) };
  t.pos = start;
  t.intValue = intVal;
  return t;
}

Token Lexer::nextToken() {
    skipWhitespace();

    size_t start = Pos;
    if (Pos >= Src.size())
        return makeToken(TokenKind::Tok_EOF, Pos, 0);

    char c = Src[Pos++];

    // Identifiers (only 'print' supported for now)
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        size_t idStart = start;
        while (Pos < Src.size()) {
            char nc = Src[Pos];
            if ((nc >= 'a' && nc <= 'z') || (nc >= 'A' && nc <= 'Z') || (nc >= '0' && nc <= '9') || nc == '_')
                ++Pos;
            else
                break;
        }
        std::string txt = Src.substr(idStart, Pos - idStart);
    if (txt == "print")
      return makeToken(TokenKind::Tok_Print, idStart, txt.size());
    if (txt == "null")
      return makeToken(TokenKind::Tok_NullLiteral, idStart, txt.size());
    if (txt == "true" || txt == "false")
      return makeToken(TokenKind::Tok_BooleanLiteral, idStart, txt.size());
    return makeToken(TokenKind::Tok_Invalid, idStart, txt.size());
    }

  // Numbers: integers, decimals, hex/binary/octal, and BigInt variants

  auto isDigit = [](char ch){ return ch >= '0' && ch <= '9'; };
  auto isHexDigit = [](char ch){ return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); };
  auto isDecDigit = [](char ch){ return (ch >= '0' && ch <= '9') || ch == '_'; };
  if (isDigit(c) || (c == '.' && Pos < Src.size() && isDigit(Src[Pos]))) {
    size_t numStart = start;

    // Handle 0-prefixed non-decimal forms: 0x, 0b, 0o
    if (c == '0' && Pos < Src.size()) {
      char nx = Src[Pos];
      // hex 0x...
      if (nx == 'x' || nx == 'X') {
        ++Pos; // consume 'x'
        // must have at least one hex digit
        if (!(Pos < Src.size() && isHexDigit(Src[Pos]))) {
          // not a valid hex literal, treat leading '0' as integer and leave 'x' for next token
          Pos = numStart + 1;
          return makeToken(TokenKind::Tok_Integer, numStart, 1, 0);
        }
        size_t p = Pos;
        while (p < Src.size() && (isHexDigit(Src[p]) || Src[p] == '_')) ++p;
        // optional trailing 'n' for BigHexIntegerLiteral
        if (p < Src.size() && Src[p] == 'n') { ++p; Pos = p; return makeToken(TokenKind::Tok_BigHexIntegerLiteral, numStart, Pos - numStart); }
        Pos = p;
        return makeToken(TokenKind::Tok_HexIntegerLiteral, numStart, Pos - numStart);
      }
      // binary 0b...
      if (nx == 'b' || nx == 'B') {
        ++Pos; // consume 'b'
        // must have at least one binary digit
        if (!(Pos < Src.size() && (Src[Pos] == '0' || Src[Pos] == '1'))) {
          Pos = numStart + 1;
          return makeToken(TokenKind::Tok_Integer, numStart, 1, 0);
        }
        size_t p = Pos;
        while (p < Src.size() && (Src[p] == '0' || Src[p] == '1' || Src[p] == '_')) ++p;
        if (p < Src.size() && Src[p] == 'n') { ++p; Pos = p; return makeToken(TokenKind::Tok_BigBinaryIntegerLiteral, numStart, Pos - numStart); }
        Pos = p;
        return makeToken(TokenKind::Tok_BinaryIntegerLiteral, numStart, Pos - numStart);
      }
      // octal with explicit 0o...
      if (nx == 'o' || nx == 'O') {
        ++Pos; // consume 'o'
        // must have at least one octal digit
        if (!(Pos < Src.size() && (Src[Pos] >= '0' && Src[Pos] <= '7'))) {
          Pos = numStart + 1;
          return makeToken(TokenKind::Tok_Integer, numStart, 1, 0);
        }
        size_t p = Pos;
        while (p < Src.size() && ((Src[p] >= '0' && Src[p] <= '7') || Src[p] == '_' )) ++p;
        if (p < Src.size() && Src[p] == 'n') { ++p; Pos = p; return makeToken(TokenKind::Tok_BigOctalIntegerLiteral, numStart, Pos - numStart); }
        Pos = p;
        return makeToken(TokenKind::Tok_OctalIntegerLiteral2, numStart, Pos - numStart);
      }

  // legacy octal: 0[0-7]+ (simple handling) -- only when not in strict mode
  if (!IsStrictMode() && Pos < Src.size() && Src[Pos] >= '0' && Src[Pos] <= '7') {
        size_t p = Pos; // we've already consumed the leading '0'
        while (p < Src.size() && (Src[p] >= '0' && Src[p] <= '7')) ++p;
        // optional trailing 'n'
        if (p < Src.size() && Src[p] == 'n') { ++p; Pos = p; return makeToken(TokenKind::Tok_BigOctalIntegerLiteral, numStart, Pos - numStart); }
        Pos = p;
        return makeToken(TokenKind::Tok_OctalIntegerLiteral, numStart, Pos - numStart);
      }
    }

    // For decimals and integers (including leading '.' form)
    bool hasDigitsBeforeDot = isDigit(c);
    if (hasDigitsBeforeDot) {
      while (Pos < Src.size() && isDecDigit(Src[Pos])) ++Pos;
    }

    bool isDecimal = false;
    // fractional part if '.' followed by digit
    if (Pos < Src.size() && Src[Pos] == '.') {
      if (Pos + 1 < Src.size() && isDigit(Src[Pos+1])) {
        isDecimal = true;
        ++Pos; // consume '.'
  while (Pos < Src.size() && isDecDigit(Src[Pos])) ++Pos;
      } else if (!hasDigitsBeforeDot) {
        // lone '.' without digits before or after -> treat as dot token, backtrack
        Pos = numStart + 1; // ensure we only consumed the '.'
        return makeToken(TokenKind::Tok_Dot, numStart, 1);
      }
    }

    // exponent part [eE][+-]?[0-9]+
    if (Pos < Src.size() && (Src[Pos] == 'e' || Src[Pos] == 'E')) {
      size_t expPos = Pos + 1;
      if (expPos < Src.size() && (Src[expPos] == '+' || Src[expPos] == '-')) ++expPos;
      if (expPos < Src.size() && isDigit(Src[expPos])) {
        ++Pos; // consume 'e' or 'E'
        if (Pos < Src.size() && (Src[Pos] == '+' || Src[Pos] == '-')) ++Pos;
  while (Pos < Src.size() && isDecDigit(Src[Pos])) ++Pos;
        isDecimal = true;
      }
    }

    // big-int suffix for decimal integer: 'n'
    if (!isDecimal && hasDigitsBeforeDot && Pos < Src.size() && Src[Pos] == 'n') {
      ++Pos; return makeToken(TokenKind::Tok_BigDecimalIntegerLiteral, numStart, Pos - numStart);
    }

    if (isDecimal) {
      return makeToken(TokenKind::Tok_DecimalLiteral, numStart, Pos - numStart);
    }

    if (hasDigitsBeforeDot) {
      // parse integer value for Tok_Integer (no overflow checks)
      int64_t val = 0;
      for (size_t i = numStart; i < Pos; ++i) {
        char ch = Src[i];
        if (ch == '_') continue; // skip numeric separators
        val = val * 10 + (ch - '0');
      }
      return makeToken(TokenKind::Tok_Integer, numStart, Pos - numStart, val);
    }
  }

    switch (c) {
  case '#': return makeToken(TokenKind::Tok_Hashtag, start, 1);
    case '[': return makeToken(TokenKind::Tok_LBracket, start, 1);
    case ']': return makeToken(TokenKind::Tok_RBracket, start, 1);
    case '(': return makeToken(TokenKind::Tok_LParen, start, 1);
    case ')': return makeToken(TokenKind::Tok_RParen, start, 1);
    case '{': return makeToken(TokenKind::Tok_LBrace, start, 1);
    case '}': return makeToken(TokenKind::Tok_RBrace, start, 1);
    case ';': return makeToken(TokenKind::Tok_Semi, start, 1);
    case ',': return makeToken(TokenKind::Tok_Comma, start, 1);
    case '=':
        // Prefer '=>' (arrow) first, then '===' over '==' and then single '=' assignment
        if (Pos < Src.size() && Src[Pos] == '>') {
          ++Pos; // consume '>'
          return makeToken(TokenKind::Tok_Arrow, start, 2);
        }
        if (Pos + 1 < Src.size() && Src[Pos] == '=' && Src[Pos+1] == '=') {
          Pos += 2; // consume '==' after the first '='
          return makeToken(TokenKind::Tok_IdentityEquals, start, 3);
        }
        if (Pos < Src.size() && Src[Pos] == '=') {
          ++Pos; // consume second '='
          return makeToken(TokenKind::Tok_Equals, start, 2);
        }
        return makeToken(TokenKind::Tok_Assign, start, 1);
  case '+':
    // Prefer '+=' over '++' and single '+'
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_PlusAssign, start, 2);
    }
    // PlusPlus '++' or single '+'
    if (Pos < Src.size() && Src[Pos] == '+') {
      ++Pos; // consume second '+'
      return makeToken(TokenKind::Tok_PlusPlus, start, 2);
    }
    return makeToken(TokenKind::Tok_Plus, start, 1);
  case ':': return makeToken(TokenKind::Tok_Colon, start, 1);
    case '.':
        // Ellipsis '...'
        if (Pos + 1 < Src.size() && Src[Pos] == '.' && Src[Pos + 1] == '.') {
            Pos += 2; // consume the next two dots
            return makeToken(TokenKind::Tok_Ellipsis, start, 3);
        }
        return makeToken(TokenKind::Tok_Dot, start, 1);
  case '?':
    // Prefer '??=' (nullish-coalescing-assign) then '??' (nullish coalesce) over '?.' (optional chain) and single '?'
    if (Pos + 1 < Src.size() && Src[Pos] == '?' && Src[Pos+1] == '=') {
      Pos += 2; // consume '?=' after first '?'
      return makeToken(TokenKind::Tok_NullishCoalescingAssign, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '?') {
      ++Pos; // consume second '?'
      return makeToken(TokenKind::Tok_NullCoalesce, start, 2);
    }
    // If next char is '.', emit QuestionDot
    if (Pos < Src.size() && Src[Pos] == '.') {
      ++Pos; // consume '.'
      return makeToken(TokenKind::Tok_QuestionDot, start, 2);
    }
    return makeToken(TokenKind::Tok_Question, start, 1);
  case '-':
    // Prefer '-=' (minus-assign) over '--' (decrement) and single '-'
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_MinusAssign, start, 2);
    }
    // MinusMinus '--' or single '-'
    if (Pos < Src.size() && Src[Pos] == '-') {
      ++Pos; // consume second '-'
      return makeToken(TokenKind::Tok_MinusMinus, start, 2);
    }
    return makeToken(TokenKind::Tok_Minus, start, 1);
  case '~': return makeToken(TokenKind::Tok_BitNot, start, 1);
  case '!':
    // Prefer '!==' over '!=' and then '!'
    if (Pos + 1 < Src.size() && Src[Pos] == '=' && Src[Pos+1] == '=') {
      Pos += 2; // consume '==' after '!'
      return makeToken(TokenKind::Tok_IdentityNotEquals, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_NotEquals, start, 2);
    }
    return makeToken(TokenKind::Tok_Not, start, 1);
  case '*':
    // Prefer '**=' (power-assign), then '**' (power), then '*=' (multiply-assign), then single '*'
    if (Pos + 1 < Src.size() && Src[Pos] == '*' && Src[Pos+1] == '=') {
      Pos += 2; // consumed '**='
      return makeToken(TokenKind::Tok_PowerAssign, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '*') {
      ++Pos; // consume second '*'
      return makeToken(TokenKind::Tok_Power, start, 2);
    }
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_MultiplyAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_Multiply, start, 1);
  case '/':
    // '/=' (divide-assign) or '/' (divide)
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_DivideAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_Divide, start, 1);
  case '%':
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_ModulusAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_Modulus, start, 1);
  case '>':
    // Handle >>>= (RightShiftLogicalAssign), >>> (RightShiftLogical), >>= (RightShiftArithmeticAssign), >> (RightShiftArithmetic), >=, >
    if (Pos + 2 < Src.size() && Src[Pos] == '>' && Src[Pos+1] == '>' && Src[Pos+2] == '=') {
      Pos += 3; // consumed '>>>='
      return makeToken(TokenKind::Tok_RightShiftLogicalAssign, start, 4);
    }
    if (Pos + 1 < Src.size() && Src[Pos] == '>' && Src[Pos+1] == '>') {
      // '>>' followed by '>' -> '>>>' (logical)
      if (Pos + 2 < Src.size() && Src[Pos+2] == '>') {
        // consume '>>>'
        Pos += 3;
        return makeToken(TokenKind::Tok_RightShiftLogical, start, 3);
      }
    }
    // Check for '>>=' (arithmetic assign)
    if (Pos < Src.size() && Src[Pos] == '>' && Pos + 1 < Src.size() && Src[Pos+1] == '=') {
      Pos += 2; // consume '>>='
      return makeToken(TokenKind::Tok_RightShiftArithmeticAssign, start, 3);
    }
    // Check for '>>' (arithmetic)
    if (Pos < Src.size() && Src[Pos] == '>') {
      ++Pos; // consume second '>'
      return makeToken(TokenKind::Tok_RightShiftArithmetic, start, 2);
    }
    // Check for '>='
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_GreaterThanEquals, start, 2);
    }
    return makeToken(TokenKind::Tok_MoreThan, start, 1);
  case '&':
    // Prefer '&&' (logical and) and '&=' (bit-and-assign) over single '&'
    if (Pos < Src.size() && Src[Pos] == '&') {
      ++Pos; // consume second '&'
      return makeToken(TokenKind::Tok_LogicalAnd, start, 2);
    }
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_BitAndAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_BitAnd, start, 1);
  case '^':
    // '^=' (bit-xor-assign) or '^' (bit-xor)
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_BitXorAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_BitXor, start, 1);
  case '|':
    // Prefer '||' (logical or) and '|=' (bit-or-assign) over single '|'
    if (Pos < Src.size() && Src[Pos] == '|') {
      ++Pos; // consume second '|'
      return makeToken(TokenKind::Tok_LogicalOr, start, 2);
    }
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_BitOrAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_BitOr, start, 1);
  case '<':
    // Handle <<= (LeftShiftArithmeticAssign), << (LeftShiftArithmetic), <=, <
    if (Pos + 1 < Src.size() && Src[Pos] == '<' && Src[Pos+1] == '=') {
      Pos += 2; // consumed '<<='
      return makeToken(TokenKind::Tok_LeftShiftArithmeticAssign, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '<') {
      ++Pos; // consume second '<'
      return makeToken(TokenKind::Tok_LeftShiftArithmetic, start, 2);
    }
    if (Pos < Src.size() && Src[Pos] == '=') {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_LessThanEquals, start, 2);
    }
    return makeToken(TokenKind::Tok_LessThan, start, 1);
    default:
        return makeToken(TokenKind::Tok_Invalid, start, 1);
    }
}

// tokenToString is implemented in token.cpp

bool Lexer::skipMultiLineComment() {
  if (Src[Pos] != '/' || Pos + 1 >= Src.size() || Src[Pos + 1] != '*') return false;
  Pos += 2; // skip '/*'
  int depth = 1;
  while (Pos + 1 < Src.size()) {
    // nested comment start
    if (Src[Pos] == '/' && Src[Pos+1] == '*') {
      depth++; Pos += 2; continue;
    }
    // recognize end '*/'
    if (Src[Pos] == '*' && Src[Pos+1] == '/') {
      Pos += 2;
      if (--depth == 0) return true;
      continue;
    }
    // advance one code unit (UTF-8 sequences are treated as bytes here)
    ++Pos;
  }
  // unterminated comment: consume to EOF and return true
  Pos = Src.size();
  return true;
}
