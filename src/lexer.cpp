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
        return makeToken(TokenKind::Tok_Invalid, idStart, txt.size());
    }

    // Numbers (integers only)
    if (c >= '0' && c <= '9') {
        size_t numStart = start;
        int64_t val = c - '0';
        while (Pos < Src.size() && Src[Pos] >= '0' && Src[Pos] <= '9') {
            val = val * 10 + (Src[Pos++] - '0');
        }
        return makeToken(TokenKind::Tok_Integer, numStart, Pos - numStart, val);
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
    case '=': return makeToken(TokenKind::Tok_Assign, start, 1);
  case '+':
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
    // Prefer NullCoalesce '??' over QuestionDot '?.' and single '?'
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
    // MinusMinus '--' or single '-'
    if (Pos < Src.size() && Src[Pos] == '-') {
      ++Pos; // consume second '-'
      return makeToken(TokenKind::Tok_MinusMinus, start, 2);
    }
    return makeToken(TokenKind::Tok_Minus, start, 1);
  case '~': return makeToken(TokenKind::Tok_BitNot, start, 1);
  case '!': return makeToken(TokenKind::Tok_Not, start, 1);
  case '*':
    // Power '**' or single '*'
    if (Pos < Src.size() && Src[Pos] == '*') {
      ++Pos; // consume second '*'
      return makeToken(TokenKind::Tok_Power, start, 2);
    }
    return makeToken(TokenKind::Tok_Multiply, start, 1);
  case '/': return makeToken(TokenKind::Tok_Divide, start, 1);
  case '%': return makeToken(TokenKind::Tok_Modulus, start, 1);
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
