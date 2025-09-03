#include "lexer.h"
#include "token.h"
#include <cctype>
#include <sstream>

// Helper: detect UTF-8 encoded U+2028 (E2 80 A8) and U+2029 (E2 80 A9)
static inline bool isUtf8LineSeparator(const std::string &s, size_t pos)
{
  if (pos + 2 >= s.size())
    return false;
  unsigned char a = static_cast<unsigned char>(s[pos]);
  unsigned char b = static_cast<unsigned char>(s[pos + 1]);
  unsigned char c = static_cast<unsigned char>(s[pos + 2]);
  return a == 0xE2 && b == 0x80 && (c == 0xA8 || c == 0xA9);
}

// Helper: detect UTF-8 encoded U+200C (E2 80 8C) or U+200D (E2 80 8D)
static inline bool isUtf8ZWNCorZWJ(const std::string &s, size_t pos)
{
  if (pos + 2 >= s.size())
    return false;
  unsigned char a = static_cast<unsigned char>(s[pos]);
  unsigned char b = static_cast<unsigned char>(s[pos + 1]);
  unsigned char c = static_cast<unsigned char>(s[pos + 2]);
  return a == 0xE2 && b == 0x80 && (c == 0x8C || c == 0x8D);
}

// Return expected UTF-8 sequence length given the lead byte.
// If not a valid lead byte, returns 1 (treat as single byte to avoid infinite loops).
static inline int utf8SequenceLength(unsigned char lead)
{
  if ((lead & 0x80) == 0)
    return 1;
  if ((lead & 0xE0) == 0xC0)
    return 2;
  if ((lead & 0xF0) == 0xE0)
    return 3;
  if ((lead & 0xF8) == 0xF0)
    return 4;
  return 1;
}

// Handle CRLF as a single line terminator
static inline size_t lineTerminatorLength(const std::string &s, size_t pos)
{
  if (pos >= s.size())
    return 0;
  unsigned char ch = static_cast<unsigned char>(s[pos]);
  if (ch == '\r')
  {
    if (pos + 1 < s.size() && static_cast<unsigned char>(s[pos + 1]) == '\n')
      return 2;
    return 1;
  }
  if (ch == '\n')
    return 1;
  if (isUtf8LineSeparator(s, pos))
    return 3;
  return 0;
}

// Regular expression helpers (match grammar fragments RegularExpressionBackslashSequence,
// RegularExpressionClassChar, RegularExpressionFirstChar, RegularExpressionChar)

// If there's a backslash escape sequence at pos (pos points at '\\'), and the next code unit
// is not a line terminator, return total length (backslash + code unit length), otherwise 0.
static inline size_t regBackslashSequenceLength(const std::string &s, size_t pos)
{
  if (pos + 1 >= s.size())
    return 0;
  // next must not be a line terminator
  if (lineTerminatorLength(s, pos + 1))
    return 0;
  unsigned char lead = static_cast<unsigned char>(s[pos + 1]);
  int len = utf8SequenceLength(lead);
  // ensure we have enough bytes
  if (pos + 1 + (len - 1) >= s.size())
    return 0;
  return 1 + len;
}

// If there's a character class starting at pos (pos points at '['), return the length up to and including
// the matching ']' (handles backslash escapes inside the class). Returns 0 if unterminated or invalid.
static inline size_t regClassLength(const std::string &s, size_t pos)
{
  if (pos >= s.size() || s[pos] != '[')
    return 0;
  size_t p = pos + 1;
  while (p < s.size())
  {
    if (s[p] == ']')
      return p - pos + 1;
    if (s[p] == '\\')
    {
      size_t blen = regBackslashSequenceLength(s, p);
      if (!blen)
        return 0; // invalid backslash escape inside class
      p += blen;
      continue;
    }
    // disallow raw line terminator inside class
    if (lineTerminatorLength(s, p))
      return 0;
    // advance by one or by UTF-8 sequence length
    unsigned char uc = static_cast<unsigned char>(s[p]);
    if (uc & 0x80)
    {
      int len = utf8SequenceLength(uc);
      p += len;
    }
    else
    {
      ++p;
    }
  }
  return 0; // unterminated
}

// RegularExpressionFirstChar: backslash sequence, class, or a non-line-terminator char (except '/').
static inline size_t regFirstCharLength(const std::string &s, size_t pos)
{
  if (pos >= s.size())
    return 0;
  if (s[pos] == '[')
    return regClassLength(s, pos);
  if (s[pos] == '\\')
    return regBackslashSequenceLength(s, pos);
  if (lineTerminatorLength(s, pos))
    return 0;
  if (s[pos] == '/')
    return 0;
  unsigned char lead = static_cast<unsigned char>(s[pos]);
  int len = utf8SequenceLength(lead);
  if (pos + len - 1 >= s.size())
    return 0;
  return len;
}

// RegularExpressionChar is same as first char for our purposes.
static inline size_t regCharLength(const std::string &s, size_t pos)
{
  return regFirstCharLength(s, pos);
}

// Grammar fragment LineContinuation: '\\' [\r\n\u2028\u2029]+
// If there's a line continuation starting at `pos` (which must point at the backslash),
// return the total byte length of the continuation (including the backslash). Otherwise return 0.
static inline size_t lineContinuationLength(const std::string &s, size_t pos)
{
  if (pos >= s.size() || s[pos] != '\\')
    return 0;
  size_t p = pos + 1;
  size_t lt = lineTerminatorLength(s, p);
  if (!lt)
    return 0;
  size_t consumed = 1; // for the backslash
  while (p < s.size())
  {
    size_t l2 = lineTerminatorLength(s, p);
    if (!l2)
      break;
    consumed += l2;
    p += l2;
  }
  return consumed;
}

// Grammar fragment HexDigit: [_0-9a-fA-F] (allow underscore per grammar fragments)
static inline bool isHexDigitFragment(unsigned char ch)
{
  if (ch == '_')
    return true;
  if (ch >= '0' && ch <= '9')
    return true;
  if (ch >= 'a' && ch <= 'f')
    return true;
  if (ch >= 'A' && ch <= 'F')
    return true;
  return false;
}

// Matches grammar fragment SingleEscapeCharacter: ["\\bfnrtv]
static inline bool isSingleEscapeChar(char ch)
{
  return ch == '"' || ch == '\\' || ch == 'b' || ch == 'f' || ch == 'n' || ch == 'r' || ch == 't' || ch == 'v';
}

void Lexer::skipWhitespace()
{
  while (Pos < Src.size())
  {
    unsigned char c = (unsigned char)Src[Pos];
    // Match the grammar's WhiteSpaces: [\t\u000B\u000C\u0020\u00A0]
    // Use explicit checks instead of isspace() so we reliably include NO-BREAK SPACE (0xA0)
    // and avoid accidentally consuming line terminators here (they are handled below).
    if (c == '\t' || c == '\v' || c == '\f' || c == ' ' || c == 0xA0)
    {
      ++Pos;
      continue;
    }

    // treat Unicode line separators (U+2028/U+2029) same as newline
    size_t ltlen = lineTerminatorLength(Src, Pos);
    if (ltlen)
    {
      Pos += ltlen;
      continue;
    }

    // try hash-bang (only allowed at start)
    if (skipHashBang())
      continue;

    // try comment skipping
    if (skipSingleLineComment())
      continue;
    // try HTML comment <!-- ... -->
    if (skipHtmlComment())
      continue;
    // try CDATA comment <![CDATA[ ... ]]> (treat like hidden channel)
    if (skipCDataComment())
      continue;
    // try multi-line comment
    if (skipMultiLineComment())
      continue;

    break; // non-whitespace, non-comment char
  }
}

bool Lexer::skipSingleLineComment()
{
  if (Src[Pos] != '/' || Pos + 1 >= Src.size() || Src[Pos + 1] != '/')
    return false;
  Pos += 2; // skip '//'
  while (Pos < Src.size())
  {
    size_t lt = lineTerminatorLength(Src, Pos);
    if (lt)
      break;
    ++Pos;
  }
  return true;
}

bool Lexer::skipHashBang()
{
  // only at start of file
  if (Pos != 0)
    return false;
  // allow optional UTF-8 BOM (0xEF 0xBB 0xBF) before the '#!'
  size_t checkPos = Pos;
  if (Src.size() >= 3 && static_cast<unsigned char>(Src[0]) == 0xEF && static_cast<unsigned char>(Src[1]) == 0xBB && static_cast<unsigned char>(Src[2]) == 0xBF)
    checkPos = 3;
  if (checkPos + 1 >= Src.size() || Src[checkPos] != '#' || Src[checkPos + 1] != '!')
    return false;
  Pos = checkPos + 2; // skip '#!' (and BOM if present by advancing past it)
  while (Pos < Src.size())
  {
    size_t lt = lineTerminatorLength(Src, Pos);
    if (lt)
      break;
    ++Pos;
  }
  return true;
}

bool Lexer::skipHtmlComment()
{
  if (Pos + 3 >= Src.size())
    return false;
  // match '<!--'
  if (Src[Pos] != '<' || Src[Pos + 1] != '!' || Src[Pos + 2] != '-' || Src[Pos + 3] != '-')
    return false;
  Pos += 4; // consume '<!--'
  // non-greedy up to '-->' or EOF
  while (Pos + 2 < Src.size())
  {
    if (Src[Pos] == '-' && Src[Pos + 1] == '-' && Src[Pos + 2] == '>')
    {
      Pos += 3; // consume '-->'
      return true;
    }
    ++Pos;
  }
  // unterminated: consume to EOF
  Pos = Src.size();
  return true;
}

bool Lexer::skipCDataComment()
{
  // match '<![CDATA['
  const char *pat = "<![CDATA[";
  const size_t patlen = 9; // length of '<![CDATA['
  if (Pos + patlen - 1 >= Src.size())
    return false;
  for (size_t i = 0; i < patlen; ++i)
  {
    if (Src[Pos + i] != pat[i])
      return false;
  }
  Pos += patlen; // consume '<![CDATA['
  while (Pos + 2 < Src.size())
  {
    if (Src[Pos] == ']' && Src[Pos + 1] == ']' && Src[Pos + 2] == '>')
    {
      Pos += 3; // consume ']]>'
      return true;
    }
    ++Pos;
  }
  // unterminated: consume to EOF
  Pos = Src.size();
  return true;
}

Token Lexer::makeToken(TokenKind k, size_t start, size_t len, std::optional<int64_t> intVal) const
{
  Token t{k, Src.substr(start, len)};
  t.pos = start;
  t.intValue = intVal;
  return t;
}

void Lexer::ProcessTemplateOpenBrace()
{
  // entering an expression inside a template: mark that we're no longer scanning template atoms
  InTemplateString = false;
}

void Lexer::ProcessTemplateCloseBrace()
{
  // when closing an expression inside a template, resume template string scanning
  InTemplateString = true;
}

bool Lexer::IsRegexPossible() const
{
  // simple heuristic: if at start of file, allow regex
  if (Pos == 0)
    return true;
  // look back for previous non-whitespace character
  long p = static_cast<long>(Pos) - 1;
  while (p >= 0)
  {
    unsigned char ch = static_cast<unsigned char>(Src[p]);
    // skip whitespace
    if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f' || ch == 0xA0)
    {
      --p;
      continue;
    }
    // skip line terminators
    size_t lt = lineTerminatorLength(Src, p);
    if (lt)
    {
      p -= static_cast<long>(lt);
      continue;
    }
    // characters that usually allow a regex to follow
    if (ch == '(' || ch == ',' || ch == '=' || ch == ':' || ch == '[' || ch == '!' || ch == '?' || ch == '{' || ch == '}')
      return true;
    // otherwise assume division
    return false;
  }
  return true;
}

bool Lexer::ContainsLineTerminatorBetween(size_t from, size_t to) const
{
  if (from >= Src.size()) return false;
  size_t p = from;
  while (p < to && p < Src.size()) {
    size_t lt = lineTerminatorLength(Src, p);
    if (lt) return true;
    // advance by UTF-8 sequence length for non-ASCII too
    unsigned char uc = static_cast<unsigned char>(Src[p]);
    int len = utf8SequenceLength(uc);
    p += len > 0 ? len : 1;
  }
  return false;
}

std::optional<Token> Lexer::scanRegularExpression(size_t start)
{
  // Pos currently points just after the initial '/'
  size_t p = Pos;
  // First char must match RegularExpressionFirstChar
  size_t firstLen = regFirstCharLength(Src, p);
  if (!firstLen)
    return std::nullopt; // not a regex here
  p += firstLen;

  while (p < Src.size())
  {
    if (Src[p] == '/')
    {
      // end of regex body
      ++p;
      // optional flags (IdentifierPart*). Accept ASCII identifier chars and Unicode escapes like \uXXXX or \u{...}
      while (p < Src.size())
      {
        char fc = Src[p];
        if ((fc >= 'a' && fc <= 'z') || (fc >= 'A' && fc <= 'Z') || (fc >= '0' && fc <= '9') || fc == '_' || fc == '$')
        {
          ++p;
          continue;
        }
        if (fc == '\\')
        {
          // Unicode escape in flags (e.g. \uXXXX or \u{...}) - try to consume
          if (p + 1 < Src.size() && Src[p + 1] == 'u')
          {
            size_t q = p + 2;
            if (q + 3 < Src.size() && isHexDigitFragment((unsigned char)Src[q]) && isHexDigitFragment((unsigned char)Src[q + 1]) && isHexDigitFragment((unsigned char)Src[q + 2]) && isHexDigitFragment((unsigned char)Src[q + 3]))
            {
              p = q + 4;
              continue;
            }
            if (q < Src.size() && Src[q] == '{')
            {
              size_t r = q + 1;
              while (r < Src.size() && isHexDigitFragment((unsigned char)Src[r]))
                ++r;
              if (r < Src.size() && Src[r] == '}' && r > q + 1)
              {
                p = r + 1;
                continue;
              }
            }
          }
        }
        break;
      }
      Pos = p;
      return makeToken(TokenKind::Tok_RegularExpressionLiteral, start, p - start);
    }

    // consume a regular expression char
    size_t clen = regCharLength(Src, p);
    if (!clen)
    {
      // invalid or line-terminator encountered inside regex
      Pos = p;
      return makeToken(TokenKind::Tok_Invalid, start, Pos - start);
    }
    p += clen;
  }

  // unterminated or invalid regex literal -> emit invalid token spanning from start
  Pos = p;
  return makeToken(TokenKind::Tok_Invalid, start, Pos - start);
}

Token Lexer::nextToken()
{
  skipWhitespace();

  size_t start = Pos;
  if (Pos >= Src.size())
    return makeToken(TokenKind::Tok_EOF, Pos, 0);

  // If currently inside a template string, scan TemplateStringAtom and template delimiters
  if (InTemplateString)
  {
    size_t atomStart = Pos;
    while (Pos < Src.size())
    {
      // handle escapes inside template atoms: backslash escapes and line continuations
      if (Src[Pos] == '\\')
      {
        if (Pos + 1 >= Src.size())
        {
          ++Pos; // trailing backslash at EOF
          continue;
        }
        // line continuation: backslash followed by one-or-more line terminators per grammar
        size_t lc = lineContinuationLength(Src, Pos);
        if (lc)
        {
          Pos += lc; // consume the entire continuation
          continue;
        }

        // Handle escape sequences inside template atom: \xNN, \uXXXX, \u{...}, or single '0'
        char esc = Src[Pos + 1];
        if (esc == 'x')
        {
          size_t q = Pos + 2;
          if (q + 1 < Src.size() && isHexDigitFragment((unsigned char)Src[q]) && isHexDigitFragment((unsigned char)Src[q + 1]))
          {
            Pos = q + 2;
            continue;
          }
          // invalid hex escape: include the '\\x' and emit invalid atom
          Pos += 2; // move past backslash and 'x'
          return makeToken(TokenKind::Tok_Invalid, atomStart, Pos - atomStart);
        }
        if (esc == 'u')
        {
          size_t q = Pos + 2;
          // \\uFFFF
          if (q + 3 < Src.size() && isHexDigitFragment((unsigned char)Src[q]) && isHexDigitFragment((unsigned char)Src[q + 1]) && isHexDigitFragment((unsigned char)Src[q + 2]) && isHexDigitFragment((unsigned char)Src[q + 3]))
          {
            Pos = q + 4;
            continue;
          }
          // \\u{...}
          if (q < Src.size() && Src[q] == '{')
          {
            size_t r = q + 1;
            while (r < Src.size() && isHexDigitFragment((unsigned char)Src[r]))
              ++r;
            if (r < Src.size() && Src[r] == '}' && r > q + 1)
            {
              Pos = r + 1;
              continue;
            }
          }
          // invalid unicode escape: include the '\\u' and emit invalid atom
          Pos += 2; // move past '\\u'
          return makeToken(TokenKind::Tok_Invalid, atomStart, Pos - atomStart);
        }
        if (esc == '0')
        {
          // '\0' is allowed but not followed by another digit (octal-like) -> invalid
          if (Pos + 2 < Src.size() && Src[Pos + 2] >= '0' && Src[Pos + 2] <= '9')
          {
            Pos += 2; // move past '\0'
            return makeToken(TokenKind::Tok_Invalid, atomStart, Pos - atomStart);
          }
          Pos += 2;
          continue;
        }
        // Accept digit escapes (e.g. '\1', '\2') as valid EscapeCharacter per grammar fragment
        if (esc >= '1' && esc <= '9')
        {
          Pos += 2; // consume backslash + digit
          continue;
        }
        // other escapes: validate SingleEscapeCharacter or accept other EscapeCharacter forms
        char escChar = Src[Pos + 1];
        // SingleEscapeCharacter per grammar: ['"\\bfnrtv]
        if (isSingleEscapeChar(escChar))
        {
          Pos += 2;
          continue;
        }
        // line terminators remain invalid here
        if (escChar == '\r' || escChar == '\n' || isUtf8LineSeparator(Src, Pos + 1))
        {
          Pos += 2; // include the bad escape
          return makeToken(TokenKind::Tok_Invalid, atomStart, Pos - atomStart);
        }
        // otherwise accept as EscapeCharacter (includes digits and 'x'/'u' handled above)
        Pos += 2;
        continue;
      }

      if (Src[Pos] == '`')
      {
        if (Pos > atomStart)
          return makeToken(TokenKind::Tok_TemplateStringAtom, atomStart, Pos - atomStart);
        // empty atom before closing backtick -> emit backtick below
        break;
      }
      if (Src[Pos] == '$' && Pos + 1 < Src.size() && Src[Pos + 1] == '{')
      {
        if (Pos > atomStart)
          return makeToken(TokenKind::Tok_TemplateStringAtom, atomStart, Pos - atomStart);
        // emit the '${' token
        Pos += 2;
        // notify caller that we entered an expression
        return makeToken(TokenKind::Tok_TemplateStringStartExpression, Pos - 2, 2);
      }
      ++Pos;
    }
    // ran to EOF or found delimiter at Pos
    if (Pos < Src.size() && Src[Pos] == '`')
    {
      // emit backtick token and exit template mode
      size_t start = Pos++;
      InTemplateString = false;
      return makeToken(TokenKind::Tok_BackTick, start, 1);
    }
    // EOF while in template -> emit remaining atom or invalid
    if (Pos > atomStart)
      return makeToken(TokenKind::Tok_TemplateStringAtom, atomStart, Pos - atomStart);
    return makeToken(TokenKind::Tok_Invalid, atomStart, 0);
  }

  char c = Src[Pos++];

  // Identifiers: support ASCII letters, digits, '_', '$', simple Unicode escape sequences (e.g. \\\uXXXX or \\\u{...}) and non-ASCII bytes
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$' || c == '\\' || (static_cast<unsigned char>(c) & 0x80))
  {
    size_t idStart = start;
    if (c == '\\')
      Pos = idStart;

    while (Pos < Src.size())
    {
      char nc = Src[Pos];
      if ((nc >= 'a' && nc <= 'z') || (nc >= 'A' && nc <= 'Z') || (nc >= '0' && nc <= '9') || nc == '_' || nc == '$')
      {
        ++Pos;
        continue;
      }
      unsigned char unc = static_cast<unsigned char>(nc);
      if (unc & 0x80)
      {
        // Handle multi-byte UTF-8 sequences. Accept U+200C / U+200D explicitly as identifier parts.
        if (isUtf8ZWNCorZWJ(Src, Pos))
        {
          Pos += 3; // consume 3-byte UTF-8 sequence for U+200C/U+200D
          continue;
        }
        // Otherwise advance by the UTF-8 sequence length to skip the full code point
        {
          unsigned char lead = static_cast<unsigned char>(Src[Pos]);
          int len = utf8SequenceLength(lead);
          Pos += len;
        }
        continue;
      }
      if (nc == '\\')
      {
        if (Pos + 1 < Src.size() && Src[Pos + 1] == 'u')
        {
          size_t p = Pos + 2;
          bool consumed = false;
          if (p + 3 < Src.size() && isHexDigitFragment((unsigned char)Src[p]) && isHexDigitFragment((unsigned char)Src[p + 1]) && isHexDigitFragment((unsigned char)Src[p + 2]) && isHexDigitFragment((unsigned char)Src[p + 3]))
          {
            Pos = p + 4;
            consumed = true;
          }
          else if (p < Src.size() && Src[p] == '{')
          {
            size_t r = p + 1;
            while (r < Src.size() && isHexDigitFragment((unsigned char)Src[r]))
              ++r;
            if (r < Src.size() && Src[r] == '}' && r > p + 1)
            {
              Pos = r + 1;
              consumed = true;
            }
          }
          if (consumed)
            continue;
        }
        break;
      }
      break;
    }
    std::string txt = Src.substr(idStart, Pos - idStart);
    if (txt == "print")
      return makeToken(TokenKind::Tok_Print, idStart, txt.size());
    if (txt == "break")
      return makeToken(TokenKind::Tok_Break, idStart, txt.size());
    if (txt == "do")
      return makeToken(TokenKind::Tok_Do, idStart, txt.size());
    if (txt == "instanceof")
      return makeToken(TokenKind::Tok_Instanceof, idStart, txt.size());
    if (txt == "typeof")
      return makeToken(TokenKind::Tok_Typeof, idStart, txt.size());
    if (txt == "case")
      return makeToken(TokenKind::Tok_Case, idStart, txt.size());
    if (txt == "else")
      return makeToken(TokenKind::Tok_Else, idStart, txt.size());
    if (txt == "new")
      return makeToken(TokenKind::Tok_New, idStart, txt.size());
    if (txt == "var")
      return makeToken(TokenKind::Tok_Var, idStart, txt.size());
    if (txt == "catch")
      return makeToken(TokenKind::Tok_Catch, idStart, txt.size());
    if (txt == "finally")
      return makeToken(TokenKind::Tok_Finally, idStart, txt.size());
    if (txt == "return")
      return makeToken(TokenKind::Tok_Return, idStart, txt.size());
    if (txt == "void")
      return makeToken(TokenKind::Tok_Void, idStart, txt.size());
    if (txt == "continue")
      return makeToken(TokenKind::Tok_Continue, idStart, txt.size());
    if (txt == "for")
      return makeToken(TokenKind::Tok_For, idStart, txt.size());
    if (txt == "switch")
      return makeToken(TokenKind::Tok_Switch, idStart, txt.size());
    if (txt == "while")
      return makeToken(TokenKind::Tok_While, idStart, txt.size());
    if (txt == "debugger")
      return makeToken(TokenKind::Tok_Debugger, idStart, txt.size());
    if (txt == "function")
      return makeToken(TokenKind::Tok_Function, idStart, txt.size());
    if (txt == "delete")
      return makeToken(TokenKind::Tok_Delete, idStart, txt.size());
    if (txt == "in")
      return makeToken(TokenKind::Tok_In, idStart, txt.size());
    if (txt == "try")
      return makeToken(TokenKind::Tok_Try, idStart, txt.size());
    if (txt == "default")
      return makeToken(TokenKind::Tok_Default, idStart, txt.size());
    if (txt == "as")
      return makeToken(TokenKind::Tok_As, idStart, txt.size());
    if (txt == "from")
      return makeToken(TokenKind::Tok_From, idStart, txt.size());
    if (txt == "of")
      return makeToken(TokenKind::Tok_Of, idStart, txt.size());
    if (txt == "yield")
      return makeToken(TokenKind::Tok_Yield, idStart, txt.size());
    if (txt == "yield*")
      return makeToken(TokenKind::Tok_YieldStar, idStart, txt.size());
    if (txt == "class")
      return makeToken(TokenKind::Tok_Class, idStart, txt.size());
    if (txt == "enum")
      return makeToken(TokenKind::Tok_Enum, idStart, txt.size());
    if (txt == "extends")
      return makeToken(TokenKind::Tok_Extends, idStart, txt.size());
    if (txt == "super")
      return makeToken(TokenKind::Tok_Super, idStart, txt.size());
    if (txt == "const")
      return makeToken(TokenKind::Tok_Const, idStart, txt.size());
    if (txt == "export")
      return makeToken(TokenKind::Tok_Export, idStart, txt.size());
    if (txt == "import")
      return makeToken(TokenKind::Tok_Import, idStart, txt.size());
    if (txt == "async")
      return makeToken(TokenKind::Tok_Async, idStart, txt.size());
    if (txt == "await")
      return makeToken(TokenKind::Tok_Await, idStart, txt.size());
    if (txt == "implements" && IsStrictMode())
      return makeToken(TokenKind::Tok_Implements, idStart, txt.size());
    if (txt == "private" && IsStrictMode())
      return makeToken(TokenKind::Tok_Private, idStart, txt.size());
    if (txt == "public" && IsStrictMode())
      return makeToken(TokenKind::Tok_Public, idStart, txt.size());
    if (txt == "interface" && IsStrictMode())
      return makeToken(TokenKind::Tok_Interface, idStart, txt.size());
    if (txt == "package" && IsStrictMode())
      return makeToken(TokenKind::Tok_Package, idStart, txt.size());
    if (txt == "protected" && IsStrictMode())
      return makeToken(TokenKind::Tok_Protected, idStart, txt.size());
    if (txt == "static" && IsStrictMode())
      return makeToken(TokenKind::Tok_Static, idStart, txt.size());
    if (txt == "let")
    {
      if (IsStrictMode())
        return makeToken(TokenKind::Tok_StrictLet, idStart, txt.size());
      return makeToken(TokenKind::Tok_NonStrictLet, idStart, txt.size());
    }
    if (txt == "null")
      return makeToken(TokenKind::Tok_NullLiteral, idStart, txt.size());
    if (txt == "true" || txt == "false")
      return makeToken(TokenKind::Tok_BooleanLiteral, idStart, txt.size());
    return makeToken(TokenKind::Tok_Invalid, idStart, txt.size());
  }

  // Numbers: integers, decimals, hex/binary/octal, and BigInt variants
  auto isDigit = [](char ch)
  { return ch >= '0' && ch <= '9'; };
  auto isHexDigit = [](char ch)
  { return isHexDigitFragment(static_cast<unsigned char>(ch)); };
  auto isDecDigit = [](char ch)
  { return (ch >= '0' && ch <= '9') || ch == '_'; };
  if (isDigit(c) || (c == '.' && Pos < Src.size() && isDigit(Src[Pos])))
  {
    size_t numStart = start;

    // Handle 0-prefixed non-decimal forms: 0x, 0b, 0o
    if (c == '0' && Pos < Src.size())
    {
      char nx = Src[Pos];
      // hex 0x...
      if (nx == 'x' || nx == 'X')
      {
        ++Pos; // consume 'x'
        // must have at least one hex digit
        if (!(Pos < Src.size() && isHexDigit(Src[Pos])))
        {
          // not a valid hex literal, treat leading '0' as integer and leave 'x' for next token
          Pos = numStart + 1;
          return makeToken(TokenKind::Tok_Integer, numStart, 1, 0);
        }
        size_t p = Pos;
        while (p < Src.size() && (isHexDigit(Src[p]) || Src[p] == '_'))
          ++p;
        // optional trailing 'n' for BigHexIntegerLiteral
        if (p < Src.size() && Src[p] == 'n')
        {
          ++p;
          Pos = p;
          return makeToken(TokenKind::Tok_BigHexIntegerLiteral, numStart, Pos - numStart);
        }
        Pos = p;
        return makeToken(TokenKind::Tok_HexIntegerLiteral, numStart, Pos - numStart);
      }
      // binary 0b...
      if (nx == 'b' || nx == 'B')
      {
        ++Pos; // consume 'b'
        // must have at least one binary digit
        if (!(Pos < Src.size() && (Src[Pos] == '0' || Src[Pos] == '1')))
        {
          Pos = numStart + 1;
          return makeToken(TokenKind::Tok_Integer, numStart, 1, 0);
        }
        size_t p = Pos;
        while (p < Src.size() && (Src[p] == '0' || Src[p] == '1' || Src[p] == '_'))
          ++p;
        if (p < Src.size() && Src[p] == 'n')
        {
          ++p;
          Pos = p;
          return makeToken(TokenKind::Tok_BigBinaryIntegerLiteral, numStart, Pos - numStart);
        }
        Pos = p;
        return makeToken(TokenKind::Tok_BinaryIntegerLiteral, numStart, Pos - numStart);
      }
      // octal with explicit 0o...
      if (nx == 'o' || nx == 'O')
      {
        ++Pos; // consume 'o'
        // must have at least one octal digit
        if (!(Pos < Src.size() && (Src[Pos] >= '0' && Src[Pos] <= '7')))
        {
          Pos = numStart + 1;
          return makeToken(TokenKind::Tok_Integer, numStart, 1, 0);
        }
        size_t p = Pos;
        while (p < Src.size() && ((Src[p] >= '0' && Src[p] <= '7') || Src[p] == '_'))
          ++p;
        if (p < Src.size() && Src[p] == 'n')
        {
          ++p;
          Pos = p;
          return makeToken(TokenKind::Tok_BigOctalIntegerLiteral, numStart, Pos - numStart);
        }
        Pos = p;
        return makeToken(TokenKind::Tok_OctalIntegerLiteral2, numStart, Pos - numStart);
      }

      // legacy octal: 0[0-7]+ (simple handling) -- only when not in strict mode
      if (!IsStrictMode() && Pos < Src.size() && Src[Pos] >= '0' && Src[Pos] <= '7')
      {
        size_t p = Pos; // we've already consumed the leading '0'
        while (p < Src.size() && (Src[p] >= '0' && Src[p] <= '7'))
          ++p;
        // optional trailing 'n'
        if (p < Src.size() && Src[p] == 'n')
        {
          ++p;
          Pos = p;
          return makeToken(TokenKind::Tok_BigOctalIntegerLiteral, numStart, Pos - numStart);
        }
        Pos = p;
        return makeToken(TokenKind::Tok_OctalIntegerLiteral, numStart, Pos - numStart);
      }

      // If we have a leading '0' followed by a decimal digit (e.g. '08'),
      // it's not a valid multi-digit DecimalIntegerLiteral per the grammar (DecimalIntegerLiteral: '0' | [1-9] [0-9_]*).
      // Emit the single '0' integer token and leave the following digit for the next token.
      if (c == '0' && Pos < Src.size() && Src[Pos] >= '0' && Src[Pos] <= '9')
      {
        Pos = numStart + 1;
        return makeToken(TokenKind::Tok_Integer, numStart, 1, 0);
      }
    }

    // For decimals and integers (including leading '.' form)
    bool hasDigitsBeforeDot = isDigit(c);
    if (hasDigitsBeforeDot)
    {
      while (Pos < Src.size() && isDecDigit(Src[Pos]))
        ++Pos;
    }

    bool isDecimal = false;
    // fractional part if '.' followed by digit
    if (Pos < Src.size() && Src[Pos] == '.')
    {
      if (Pos + 1 < Src.size() && isDigit(Src[Pos + 1]))
      {
        isDecimal = true;
        ++Pos; // consume '.'
        while (Pos < Src.size() && isDecDigit(Src[Pos]))
          ++Pos;
      }
      else if (!hasDigitsBeforeDot)
      {
        // lone '.' without digits before or after -> treat as dot token, backtrack
        Pos = numStart + 1; // ensure we only consumed the '.'
        return makeToken(TokenKind::Tok_Dot, numStart, 1);
      }
    }

    // exponent part [eE][+-]?[0-9]+
    if (Pos < Src.size() && (Src[Pos] == 'e' || Src[Pos] == 'E'))
    {
      size_t expPos = Pos + 1;
      if (expPos < Src.size() && (Src[expPos] == '+' || Src[expPos] == '-'))
        ++expPos;
      if (expPos < Src.size() && isDecDigit(Src[expPos]))
      {
        ++Pos; // consume 'e' or 'E'
        if (Pos < Src.size() && (Src[Pos] == '+' || Src[Pos] == '-'))
          ++Pos;
        while (Pos < Src.size() && isDecDigit(Src[Pos]))
          ++Pos;
        isDecimal = true;
      }
    }

    // big-int suffix for decimal integer: 'n'
    if (!isDecimal && hasDigitsBeforeDot && Pos < Src.size() && Src[Pos] == 'n')
    {
      ++Pos;
      return makeToken(TokenKind::Tok_BigDecimalIntegerLiteral, numStart, Pos - numStart);
    }

    if (isDecimal)
    {
      return makeToken(TokenKind::Tok_DecimalLiteral, numStart, Pos - numStart);
    }

    if (hasDigitsBeforeDot)
    {
      // parse integer value for Tok_Integer (no overflow checks)
      int64_t val = 0;
      for (size_t i = numStart; i < Pos; ++i)
      {
        char ch = Src[i];
        if (ch == '_')
          continue; // skip numeric separators
        val = val * 10 + (ch - '0');
      }
      return makeToken(TokenKind::Tok_Integer, numStart, Pos - numStart, val);
    }
  }

  // String literals: '...' or "..." (basic handling with escapes, no full Unicode validation)
  if (c == '"' || c == '\'')
  {
    char quote = c;
    size_t p = Pos; // current position is after opening quote
    bool terminated = false;
    while (p < Src.size())
    {
      char ch = Src[p++];
      if (ch == quote)
      {
        terminated = true;
        break;
      }
      if (ch == '\\')
      {
        // line continuation per grammar: backslash followed by one-or-more line terminators
        size_t lc = lineContinuationLength(Src, p - 1); // p-1 points at the backslash
        if (lc)
        {
          p += (lc - 1); // we've already advanced p past the backslash, so add remaining bytes
          continue;      // line continuation: string continues
        }

        // Handle escape sequences: \xHH, \uXXXX, \u{...} or any single escaped char
        if (p < Src.size())
        {
          char esc = Src[p];
          // hex escape \xNN
          if (esc == 'x')
          {
            size_t q = p + 1;
            if (q + 1 < Src.size() && isHexDigitFragment((unsigned char)Src[q]) && isHexDigitFragment((unsigned char)Src[q + 1]))
            {
              p = q + 2;
              continue;
            }
            // invalid hex escape: produce invalid token
            ++q; // include at least the 'x'
            size_t badLen = q - start;
            Pos = start + badLen;
            return makeToken(TokenKind::Tok_Invalid, start, badLen);
          }
          // unicode escape \uXXXX or \u{...}
          if (esc == 'u')
          {
            size_t q = p + 1;
            // \uFFFF
            if (q + 3 < Src.size() && isHexDigitFragment((unsigned char)Src[q]) && isHexDigitFragment((unsigned char)Src[q + 1]) && isHexDigitFragment((unsigned char)Src[q + 2]) && isHexDigitFragment((unsigned char)Src[q + 3]))
            {
              p = q + 4;
              continue;
            }
            // \u{...}
            if (q < Src.size() && Src[q] == '{')
            {
              size_t r = q + 1;
              while (r < Src.size() && isHexDigitFragment((unsigned char)Src[r]))
                ++r;
              if (r < Src.size() && Src[r] == '}' && r > q + 1)
              {
                p = r + 1;
                continue;
              }
            }
            // invalid unicode escape: produce invalid token
            ++q; // include the 'u'
            size_t badLen = q - start;
            Pos = start + badLen;
            return makeToken(TokenKind::Tok_Invalid, start, badLen);
          }

          // Other escapes: validate SingleEscapeCharacter or accept digit escapes
          // Special-case '\0' followed by digit -> invalid
          if (Src[p] == '0' && p + 1 < Src.size() && Src[p + 1] >= '0' && Src[p + 1] <= '9')
          {
            // invalid octal-like escape inside string
            size_t badLen = p + 1 - start; // include backslash and '0'
            Pos = start + badLen;
            return makeToken(TokenKind::Tok_Invalid, start, badLen);
          }
          char escChar2 = Src[p];
          if (isSingleEscapeChar(escChar2))
          {
            ++p;
            continue;
          }
          // Accept digit escapes (\1..\9) as valid EscapeCharacter
          if (escChar2 >= '1' && escChar2 <= '9')
          {
            ++p; // consume the digit
            continue;
          }
          // line terminators remain invalid
          if (escChar2 == '\r' || escChar2 == '\n' || isUtf8LineSeparator(Src, p))
          {
            size_t badLen = p + 1 - start;
            Pos = start + badLen;
            return makeToken(TokenKind::Tok_Invalid, start, badLen);
          }
          // other characters (including other NonEscapeCharacter) are accepted
          ++p;
        }
        continue;
      }

      // treat raw line terminators as unterminated string
      size_t lt = lineTerminatorLength(Src, p - 1);
      if (lt)
      {
        // unterminated string literal spans into a newline -> invalid token
        terminated = false;
        break;
      }
    }
    size_t len = (terminated ? (p - start) : (p - start));
    Pos = p;
    if (terminated)
      return makeToken(TokenKind::Tok_StringLiteral, start, len);
    return makeToken(TokenKind::Tok_Invalid, start, len);
  }

  switch (c)
  {
  case '`':
    return makeToken(TokenKind::Tok_BackTick, start, 1);

  case '#':
    return makeToken(TokenKind::Tok_Hashtag, start, 1);
  case '[':
    return makeToken(TokenKind::Tok_LBracket, start, 1);
  case ']':
    return makeToken(TokenKind::Tok_RBracket, start, 1);
  case '(':
    return makeToken(TokenKind::Tok_LParen, start, 1);
  case ')':
    return makeToken(TokenKind::Tok_RParen, start, 1);
  case '{':
    return makeToken(TokenKind::Tok_LBrace, start, 1);
  case '}':
    return makeToken(TokenKind::Tok_RBrace, start, 1);
  case ';':
    return makeToken(TokenKind::Tok_Semi, start, 1);
  case ',':
    return makeToken(TokenKind::Tok_Comma, start, 1);
  case '=':
    // Prefer '=>' (arrow) first, then '===' over '==' and then single '=' assignment
    if (Pos < Src.size() && Src[Pos] == '>')
    {
      ++Pos; // consume '>'
      return makeToken(TokenKind::Tok_Arrow, start, 2);
    }
    if (Pos + 1 < Src.size() && Src[Pos] == '=' && Src[Pos + 1] == '=')
    {
      Pos += 2; // consume '==' after the first '='
      return makeToken(TokenKind::Tok_IdentityEquals, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume second '='
      return makeToken(TokenKind::Tok_Equals, start, 2);
    }
    return makeToken(TokenKind::Tok_Assign, start, 1);
  case '+':
    // Prefer '+=' over '++' and single '+'
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_PlusAssign, start, 2);
    }
    // PlusPlus '++' or single '+'
    if (Pos < Src.size() && Src[Pos] == '+')
    {
      ++Pos; // consume second '+'
      return makeToken(TokenKind::Tok_PlusPlus, start, 2);
    }
    return makeToken(TokenKind::Tok_Plus, start, 1);
  case ':':
    return makeToken(TokenKind::Tok_Colon, start, 1);
  case '.':
    // Ellipsis '...'
    if (Pos + 1 < Src.size() && Src[Pos] == '.' && Src[Pos + 1] == '.')
    {
      Pos += 2; // consume the next two dots
      return makeToken(TokenKind::Tok_Ellipsis, start, 3);
    }
    return makeToken(TokenKind::Tok_Dot, start, 1);
  case '?':
    // Prefer '??=' (nullish-coalescing-assign) then '??' (nullish coalesce) over '?.' (optional chain) and single '?'
    if (Pos + 1 < Src.size() && Src[Pos] == '?' && Src[Pos + 1] == '=')
    {
      Pos += 2; // consume '?=' after first '?'
      return makeToken(TokenKind::Tok_NullishCoalescingAssign, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '?')
    {
      ++Pos; // consume second '?'
      return makeToken(TokenKind::Tok_NullCoalesce, start, 2);
    }
    // If next char is '.', emit QuestionDot
    if (Pos < Src.size() && Src[Pos] == '.')
    {
      ++Pos; // consume '.'
      return makeToken(TokenKind::Tok_QuestionDot, start, 2);
    }
    return makeToken(TokenKind::Tok_Question, start, 1);
  case '-':
    // Prefer '-=' (minus-assign) over '--' (decrement) and single '-'
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_MinusAssign, start, 2);
    }
    // MinusMinus '--' or single '-'
    if (Pos < Src.size() && Src[Pos] == '-')
    {
      ++Pos; // consume second '-'
      return makeToken(TokenKind::Tok_MinusMinus, start, 2);
    }
    return makeToken(TokenKind::Tok_Minus, start, 1);
  case '~':
    return makeToken(TokenKind::Tok_BitNot, start, 1);
  case '!':
    // Prefer '!==' over '!=' and then '!'
    if (Pos + 1 < Src.size() && Src[Pos] == '=' && Src[Pos + 1] == '=')
    {
      Pos += 2; // consume '==' after '!'
      return makeToken(TokenKind::Tok_IdentityNotEquals, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_NotEquals, start, 2);
    }
    return makeToken(TokenKind::Tok_Not, start, 1);
  case '*':
    // Prefer '**=' (power-assign), then '**' (power), then '*=' (multiply-assign), then single '*'
    if (Pos + 1 < Src.size() && Src[Pos] == '*' && Src[Pos + 1] == '=')
    {
      Pos += 2; // consumed '**='
      return makeToken(TokenKind::Tok_PowerAssign, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '*')
    {
      ++Pos; // consume second '*'
      return makeToken(TokenKind::Tok_Power, start, 2);
    }
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_MultiplyAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_Multiply, start, 1);
  case '/':
    if (IsRegexPossible())
    {
      auto tok = scanRegularExpression(start);
      if (tok.has_value())
        return tok.value();
      // otherwise fall through to division handling
    }

    // '/=' (divide-assign) or '/' (divide)
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_DivideAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_Divide, start, 1);
  case '%':
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_ModulusAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_Modulus, start, 1);
  case '>':
    // Handle >>>= (RightShiftLogicalAssign), >>> (RightShiftLogical), >>= (RightShiftArithmeticAssign), >> (RightShiftArithmetic), >=, >
    if (Pos + 2 < Src.size() && Src[Pos] == '>' && Src[Pos + 1] == '>' && Src[Pos + 2] == '=')
    {
      Pos += 3; // consumed '>>>='
      return makeToken(TokenKind::Tok_RightShiftLogicalAssign, start, 4);
    }
    if (Pos + 1 < Src.size() && Src[Pos] == '>' && Src[Pos + 1] == '>')
    {
      // '>>' followed by '>' -> '>>>' (logical)
      if (Pos + 2 < Src.size() && Src[Pos + 2] == '>')
      {
        // consume '>>>'
        Pos += 3;
        return makeToken(TokenKind::Tok_RightShiftLogical, start, 3);
      }
    }
    // Check for '>>=' (arithmetic assign)
    if (Pos < Src.size() && Src[Pos] == '>' && Pos + 1 < Src.size() && Src[Pos + 1] == '=')
    {
      Pos += 2; // consume '>>='
      return makeToken(TokenKind::Tok_RightShiftArithmeticAssign, start, 3);
    }
    // Check for '>>' (arithmetic)
    if (Pos < Src.size() && Src[Pos] == '>')
    {
      ++Pos; // consume second '>'
      return makeToken(TokenKind::Tok_RightShiftArithmetic, start, 2);
    }
    // Check for '>='
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_GreaterThanEquals, start, 2);
    }
    return makeToken(TokenKind::Tok_MoreThan, start, 1);
  case '&':
    // Prefer '&&' (logical and) and '&=' (bit-and-assign) over single '&'
    if (Pos < Src.size() && Src[Pos] == '&')
    {
      ++Pos; // consume second '&'
      return makeToken(TokenKind::Tok_LogicalAnd, start, 2);
    }
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_BitAndAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_BitAnd, start, 1);
  case '^':
    // '^=' (bit-xor-assign) or '^' (bit-xor)
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_BitXorAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_BitXor, start, 1);
  case '|':
    // Prefer '||' (logical or) and '|=' (bit-or-assign) over single '|'
    if (Pos < Src.size() && Src[Pos] == '|')
    {
      ++Pos; // consume second '|'
      return makeToken(TokenKind::Tok_LogicalOr, start, 2);
    }
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_BitOrAssign, start, 2);
    }
    return makeToken(TokenKind::Tok_BitOr, start, 1);
  case '<':
    // Handle <<= (LeftShiftArithmeticAssign), << (LeftShiftArithmetic), <=, <
    if (Pos + 1 < Src.size() && Src[Pos] == '<' && Src[Pos + 1] == '=')
    {
      Pos += 2; // consumed '<<='
      return makeToken(TokenKind::Tok_LeftShiftArithmeticAssign, start, 3);
    }
    if (Pos < Src.size() && Src[Pos] == '<')
    {
      ++Pos; // consume second '<'
      return makeToken(TokenKind::Tok_LeftShiftArithmetic, start, 2);
    }
    if (Pos < Src.size() && Src[Pos] == '=')
    {
      ++Pos; // consume '='
      return makeToken(TokenKind::Tok_LessThanEquals, start, 2);
    }
    return makeToken(TokenKind::Tok_LessThan, start, 1);
  default:
    return makeToken(TokenKind::Tok_Invalid, start, 1);
  }
}

// tokenToString is implemented in token.cpp

bool Lexer::skipMultiLineComment()
{
  if (Src[Pos] != '/' || Pos + 1 >= Src.size() || Src[Pos + 1] != '*')
    return false;
  Pos += 2; // skip '/*'
  int depth = 1;
  while (Pos + 1 < Src.size())
  {
    // nested comment start
    if (Src[Pos] == '/' && Src[Pos + 1] == '*')
    {
      depth++;
      Pos += 2;
      continue;
    }
    // recognize end '*/'
    if (Src[Pos] == '*' && Src[Pos + 1] == '/')
    {
      Pos += 2;
      if (--depth == 0)
        return true;
      continue;
    }
    // advance one code unit (UTF-8 sequences are treated as bytes here)
    ++Pos;
  }
  // unterminated comment: consume to EOF and return true
  Pos = Src.size();
  return true;
}
