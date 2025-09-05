#include "parser.h"
#include <optional>
#include <string>
#include <iostream>
#include <functional>

std::optional<ParseResult> Parser::parseSwitchStatement()
{
  // switchStatement : Switch '(' expressionSequence ')' caseBlock
  if (Cur.kind != TokenKind::Tok_Switch)
    return std::nullopt;
  advance();
  if (Cur.kind != TokenKind::Tok_LParen)
    return error("expected '(' after switch");
  advance();
  if (!parseExpressionSequence())
    return error("invalid switch expression");
  if (Cur.kind != TokenKind::Tok_RParen)
    return error("expected ')' after switch expression");
  advance();
  if (!parseCaseBlock())
    return error("invalid case block");
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseCaseBlock()
{
  // caseBlock : '{' caseClauses? (defaultClause caseClauses?)? '}'
  if (Cur.kind != TokenKind::Tok_LBrace)
    return false;
  advance();
  // caseClauses? (defaultClause caseClauses?)?
  parseCaseClauses(); // optional
  if (Cur.kind == TokenKind::Tok_Default)
  {
    parseDefaultClause();
    parseCaseClauses(); // optional
  }
  if (Cur.kind != TokenKind::Tok_RBrace)
    return false;
  advance();
  return true;
}

bool Parser::parseCaseClauses()
{
  // caseClauses : caseClause+
  bool any = false;
  while (true)
  {
    if (!parseCaseClause())
      break;
    any = true;
  }
  return any;
}

bool Parser::parseCaseClause()
{
  // caseClause : Case expressionSequence ':' statementList?
  if (Cur.kind != TokenKind::Tok_Case)
    return false;
  advance();
  if (!parseExpressionSequence())
    return false;
  if (Cur.kind != TokenKind::Tok_Colon)
    return false;
  advance();
  parseStatementList(); // optional
  return true;
}

bool Parser::parseDefaultClause()
{
  // defaultClause : Default ':' statementList?
  if (Cur.kind != TokenKind::Tok_Default)
    return false;
  advance();
  if (Cur.kind != TokenKind::Tok_Colon)
    return false;
  advance();
  parseStatementList(); // optional
  return true;
}

ParseResult Parser::parse()
{
  // Handle optional leading hash-bang; may return an empty-Program result
  if (auto res = handleOptionalHashBang())
    return std::move(*res);
  // Parse optional sourceElements (sourceElement*) per grammar.
  if (auto se = parseSourceElements())
  {
    // Ensure we've consumed to EOF
    if (Cur.kind != TokenKind::Tok_EOF)
    {
      // Print remaining tokens for diagnostics
      std::cerr << "Parse error: expected EOF after source elements\n";
      std::cerr << "Remaining tokens:" << std::endl;
      int count = 0;
      while (Cur.kind != TokenKind::Tok_EOF && count < 20)
      { // limit to 20 tokens for sanity
        // std::cerr << "  kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << std::endl;
        advance();
        ++count;
      }
      if (Cur.kind == TokenKind::Tok_EOF)
      {
        // std::cerr << "  kind=EOF" << std::endl;
      }
      return error("expected EOF after source elements");
    }
    return std::move(*se);
  }

  // empty program
  if (Cur.kind != TokenKind::Tok_EOF)
    return ParseResult{true, std::string(), nullptr};
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseImportDefault()
{
  // importDefault : aliasName ','
  auto save = Cur;
  if (!parseAliasName())
    return false;
  if (Cur.kind != TokenKind::Tok_Comma)
  {
    // restore (parseAliasName may have consumed tokens)
    Cur = save;
    return false;
  }
  // consume comma
  advance();
  return true;
}

bool Parser::parseImportFromBlock()
{
  // std::cerr << "DEBUG: enter parseImportFromBlock Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // importFromBlock
  //   : importDefault? (importNamespace | importModuleItems) importFrom eos
  //   | StringLiteral eos

  if (Cur.kind == TokenKind::Tok_StringLiteral)
  {
    advance();
    if (!parseEos())
      return false;
    return true;
  }

  // optional importDefault
  auto saved = Cur;
  if (parseImportDefault())
  {
    // consumed default import + comma
  }
  else
  {
    // ensure we didn't consume tokens
    Cur = saved;
  }

  // must have importNamespace or importModuleItems (or identifierName as shorthand)
  // Try to detect the forms by token kind
  if (Cur.kind == TokenKind::Tok_Multiply)
  {
    if (!parseImportNamespace())
      return false;
  }
  else if (Cur.kind == TokenKind::Tok_LBrace)
  {
    if (!parseImportModuleItems())
      return false;
  }
  else if (parseIdentifierName())
  {
    // identifierName consumed an identifier-like token; try to interpret as namespace
    // if an 'as' follows it will be handled by parseImportNamespace
    // If this isn't actually a valid import form, subsequent checks will fail.
  }
  else
  {
    return false;
  }

  // expect importFrom (the 'from' keyword)
  if (!parseImportFrom())
    return false;
  // std::cerr << "DEBUG: exit parseImportFromBlock Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return true;
}

bool Parser::parseImportNamespace()
{
  // importNamespace : ('*' | identifierName) (As identifierName)?
  if (Cur.kind == TokenKind::Tok_Multiply)
  {
    advance();
  }
  else
  {
    if (!parseIdentifierName())
      return false;
  }
  if (Cur.kind == TokenKind::Tok_As)
  {
    advance();
    if (!parseIdentifierName())
      return false;
  }
  return true;
}

bool Parser::parseImportFrom()
{
  // std::cerr << "DEBUG: enter parseImportFrom Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // importFrom : From StringLiteral
  if (Cur.kind != TokenKind::Tok_From)
    return false;
  advance();
  if (Cur.kind != TokenKind::Tok_StringLiteral)
    return false;
  advance();
  // accept semicolon or EOF as eos
  parseEos();
  // std::cerr << "DEBUG: exit parseImportFrom Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return true;
}

std::optional<ParseResult> Parser::parseImportStatement()
{
  // std::cerr << "DEBUG: enter parseImportStatement Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // importStatement : Import importFromBlock
  if (Cur.kind != TokenKind::Tok_Import)
    return std::nullopt;
  advance();
  if (!parseImportFromBlock())
    return error("invalid import statement");
  // std::cerr << "DEBUG: exit parseImportStatement Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseAliasName()
{
  // aliasName : identifierName (As identifierName)?
  if (!parseIdentifierName())
    return false;
  if (Cur.kind == TokenKind::Tok_As)
  {
    advance();
    if (!parseIdentifierName())
      return false;
  }
  return true;
}

std::optional<ParseResult> Parser::parseExportStatement()
{
  // exportStatement
  if (Cur.kind != TokenKind::Tok_Export)
    return std::nullopt;
  advance();
  // optional Default
  if (Cur.kind == TokenKind::Tok_Default)
  {
    advance();
    // Export Default singleExpression eos
    // For now, treat singleExpression as a single token/construct and accept any non-eos token sequence conservatively.
    // We'll be conservative: if we see an eos token, return success, otherwise consume one token and expect eos.
    if (parseEos())
      return ParseResult{true, std::string(), nullptr};
    // consume a token as part of the expression
    advance();
    if (!parseEos())
      return error("expected end of export default");
    return ParseResult{true, std::string(), nullptr};
  }

  // Export Default? (exportFromBlock | declaration) eos
  if (parseExportFromBlock())
    return ParseResult{true, std::string(), nullptr};
  // declaration (variable/class/function)
  if (auto decl = parseDeclaration())
  {
    // expect eos
    parseEos();
    return std::move(*decl);
  }
  return error("invalid export statement");
}

bool Parser::parseExportFromBlock()
{
  // exportFromBlock
  // : importNamespace importFrom eos
  // | exportModuleItems importFrom? eos

  // Try importNamespace importFrom eos
  auto saved = Cur;
  if (parseImportNamespace())
  {
    if (!parseImportFrom())
    {
      Cur = saved;
    }
    else
      return true;
  }

  // exportModuleItems importFrom? eos
  if (parseExportModuleItems())
  {
    // optional importFrom
    if (parseImportFrom())
      return true;
    // accept eos
    if (parseEos())
      return true;
    return false;
  }
  return false;
}

bool Parser::parseExportModuleItems()
{
  // exportModuleItems : '{' (exportAliasName ',')* (exportAliasName ','?)? '}'
  if (Cur.kind != TokenKind::Tok_LBrace)
    return false;
  advance();
  while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF)
  {
    if (!parseExportAliasName())
      return false;
    if (Cur.kind == TokenKind::Tok_Comma)
    {
      advance();
      if (Cur.kind == TokenKind::Tok_RBrace)
        break;
      continue;
    }
    break;
  }
  if (Cur.kind != TokenKind::Tok_RBrace)
    return false;
  advance();
  return true;
}

bool Parser::parseExportAliasName()
{
  // exportAliasName : moduleExportName (As moduleExportName)?
  if (!parseModuleExportName())
    return false;
  if (Cur.kind == TokenKind::Tok_As)
  {
    advance();
    if (!parseModuleExportName())
      return false;
  }
  return true;
}

bool Parser::parseModuleExportName()
{
  // moduleExportName : identifierName | StringLiteral
  if (Cur.kind == TokenKind::Tok_StringLiteral)
  {
    advance();
    return true;
  }
  // std::cerr << "DEBUG: parseModuleExportName try identifierName Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return parseIdentifierName();
}

bool Parser::parseImportModuleItems()
{
  // std::cerr << "DEBUG: enter parseImportModuleItems Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // importModuleItems : '{' (importAliasName ',')* (importAliasName ','?)? '}'
  if (Cur.kind != TokenKind::Tok_LBrace)
    return false;
  advance();
  while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF)
  {
    if (!parseImportAliasName())
      return false;
    if (Cur.kind == TokenKind::Tok_Comma)
    {
      advance();
      if (Cur.kind == TokenKind::Tok_RBrace)
        break;
      continue;
    }
    break;
  }
  if (Cur.kind != TokenKind::Tok_RBrace)
    return false;
  advance();
  // std::cerr << "DEBUG: exit parseImportModuleItems Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return true;
}

bool Parser::parseImportAliasName()
{
  // std::cerr << "DEBUG: enter parseImportAliasName Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // importAliasName : moduleExportName (As importedBinding)?
  if (!parseModuleExportName())
    return false;
  if (Cur.kind == TokenKind::Tok_As)
  {
    advance();
    if (!parseImportedBinding())
      return false;
  }
  // std::cerr << "DEBUG: exit parseImportAliasName Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return true;
}

std::optional<ParseResult> Parser::parsePrintStatement()
{
  // std::cerr << "[DEBUG] Enter parsePrintStatement: Cur.kind=" << (int)Cur.kind << " text=" << Cur.text << std::endl;
  if (Cur.kind != TokenKind::Tok_Print && Cur.kind != TokenKind::Tok_ConsoleLog && Cur.kind != TokenKind::Tok_ConsoleError && Cur.kind != TokenKind::Tok_ConsoleWarn && Cur.kind != TokenKind::Tok_ConsoleInfo && Cur.kind != TokenKind::Tok_ConsoleSuccess)
    return std::nullopt;
  // std::cout << "[parsePrintStatement] Matched print/console.log, advancing" << std::endl;
  TokenKind printKind = Cur.kind;
  advance();
  // std::cout << "[parsePrintStatement] After advance: Cur.kind=" << (int)Cur.kind << " text=" << Cur.text << std::endl;
  if (Cur.kind != TokenKind::Tok_LParen)
    return error("expected '(');");
  // std::cout << "[parsePrintStatement] Matched '(', advancing" << std::endl;
  advance();
  // std::cout << "[parsePrintStatement] After advance: Cur.kind=" << (int)Cur.kind << " text=" << Cur.text << std::endl;
  // Parse comma-separated arguments inside print(...)
  std::vector<std::unique_ptr<Expr>> args;
  bool expectArg = true;
  while (Cur.kind != TokenKind::Tok_EOF) {
    // std::cerr << "[DEBUG] print arg token: kind=" << (int)Cur.kind << " text=" << Cur.text << std::endl;
    if (Cur.kind == TokenKind::Tok_RParen) {
      advance();
      break;
    }
    if (!expectArg) {
      if (Cur.kind == TokenKind::Tok_Comma) {
        advance();
        expectArg = true;
        continue;
      } else {
        // std::cerr << "[DEBUG] parsePrintStatement error: expected ',' between print arguments\n";
        return error("expected ',' between print arguments");
      }
    }
    // Parse a single argument (literal or identifier)
    if (Cur.kind == TokenKind::Tok_StringLiteral) {
      std::string lit = Cur.text;
      if (lit.size() >= 2 && ((lit.front() == '"' && lit.back() == '"') || (lit.front() == '\'' && lit.back() == '\''))) {
        lit = lit.substr(1, lit.size() - 2);
      }
      args.push_back(std::make_unique<LiteralExpr>(lit));
      advance();
    } else if (Cur.kind == TokenKind::Tok_Number || Cur.kind == TokenKind::Tok_Integer) {
      args.push_back(std::make_unique<LiteralExpr>(Cur.text));
      advance();
    } else if (Cur.kind == TokenKind::Tok_Identifier) {
      args.push_back(std::make_unique<LiteralExpr>(Cur.text));
      advance();
    } else {
      // std::cerr << "[DEBUG] parsePrintStatement error: unsupported print argument, kind=" << (int)Cur.kind << " text=" << Cur.text << std::endl;
      return error("unsupported print argument");
    }
    expectArg = false;
  }
  // std::cerr << "[DEBUG] parsePrintStatement success: " << args.size() << " args\n";
  auto stmt = std::make_unique<PrintStmt>(std::move(args), printKind);
  return ParseResult{true, std::string(), std::move(stmt)};
}

bool Parser::parseImportedBinding()
{
  // importedBinding : Identifier | Yield | Await
  if (Cur.kind == TokenKind::Tok_Yield || Cur.kind == TokenKind::Tok_Await)
  {
    advance();
    return true;
  }
  // Identifier-like: use identifierName recognizer (conservative)
  return parseIdentifierName();
}

bool Parser::parseIdentifierName()
{
  // identifierName : identifier | reservedWord
  // First try identifier production.
  // std::cerr << "DEBUG: parseIdentifierName start Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  if (parseIdentifier())
  {
    advance();
    return true;
  }
  // std::cerr << "DEBUG: parseIdentifierName after parseIdentifier Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return parseReservedWord();
}

bool Parser::parseReservedWord()
{
  // reservedWord : keyword | NullLiteral | BooleanLiteral
  if (Cur.kind == TokenKind::Tok_NullLiteral || Cur.kind == TokenKind::Tok_BooleanLiteral ||
      Cur.kind == TokenKind::Tok_Any || Cur.kind == TokenKind::Tok_Number || Cur.kind == TokenKind::Tok_Never ||
      Cur.kind == TokenKind::Tok_String || Cur.kind == TokenKind::Tok_Unique || Cur.kind == TokenKind::Tok_Symbol ||
      Cur.kind == TokenKind::Tok_Undefined || Cur.kind == TokenKind::Tok_Object)
  {
    advance();
    return true;
  }
  return parseKeyword();
}

bool Parser::parseKeyword()
{
  switch (Cur.kind)
  {
  case TokenKind::Tok_Break:
  case TokenKind::Tok_Do:
  case TokenKind::Tok_Instanceof:
  case TokenKind::Tok_Typeof:
  case TokenKind::Tok_Case:
  case TokenKind::Tok_Else:
  case TokenKind::Tok_New:
  case TokenKind::Tok_Var:
  case TokenKind::Tok_Catch:
  case TokenKind::Tok_Finally:
  case TokenKind::Tok_Return:
  case TokenKind::Tok_Void:
  case TokenKind::Tok_Continue:
  case TokenKind::Tok_For:
  case TokenKind::Tok_Switch:
  case TokenKind::Tok_While:
  case TokenKind::Tok_Debugger:
  case TokenKind::Tok_Function:
  case TokenKind::Tok_This:
  case TokenKind::Tok_With:
  case TokenKind::Tok_Default:
  case TokenKind::Tok_If:
  case TokenKind::Tok_Throw:
  case TokenKind::Tok_Delete:
  case TokenKind::Tok_In:
  case TokenKind::Tok_Try:
  case TokenKind::Tok_Class:
  case TokenKind::Tok_Enum:
  case TokenKind::Tok_Extends:
  case TokenKind::Tok_Super:
  case TokenKind::Tok_Const:
  case TokenKind::Tok_Export:
  case TokenKind::Tok_Import:
  case TokenKind::Tok_Implements:
  case TokenKind::Tok_Private:
  case TokenKind::Tok_Public:
  case TokenKind::Tok_Interface:
  case TokenKind::Tok_Package:
  case TokenKind::Tok_Protected:
  case TokenKind::Tok_Static:
  case TokenKind::Tok_Yield:
  case TokenKind::Tok_YieldStar:
  case TokenKind::Tok_Await:
  case TokenKind::Tok_As:
  case TokenKind::Tok_From:
  case TokenKind::Tok_Of:
  case TokenKind::Tok_Async:
  case TokenKind::Tok_Any:
  case TokenKind::Tok_Number:
  case TokenKind::Tok_Never:
  case TokenKind::Tok_Boolean:
  case TokenKind::Tok_String:
  case TokenKind::Tok_Unique:
  case TokenKind::Tok_Symbol:
  case TokenKind::Tok_Undefined:
  case TokenKind::Tok_Object:
    advance();
    return true;
  default:
    return false;
  }
}

bool Parser::parseIdentifier()
{
  // identifier
  switch (Cur.kind)
  {
  case TokenKind::Tok_Identifier:
  case TokenKind::Tok_NonStrictLet:
  case TokenKind::Tok_StrictLet:
  case TokenKind::Tok_Async:
  case TokenKind::Tok_As:
  case TokenKind::Tok_From:
  case TokenKind::Tok_Yield:
  case TokenKind::Tok_Of:
    advance();
    return true;
  default:
    return false;
  }
}

std::optional<ParseResult> Parser::parseBlock()
{
  if (Cur.kind != TokenKind::Tok_LBrace)
    return std::nullopt;
  // consume '{'
  advance();
  // empty block
  if (Cur.kind == TokenKind::Tok_RBrace)
  {
    advance();
    return ParseResult{true, std::string(), nullptr};
  }
  if (auto sl = parseStatementList())
  {
    if (Cur.kind != TokenKind::Tok_RBrace)
      return error("expected '}'");
    advance();
    return std::move(*sl);
  }
  return error("invalid block contents");
}

std::optional<ParseResult> Parser::parseStatementList()
{
  // statementList : statement+
  bool any = false;
  std::optional<ParseResult> last;
  while (true)
  {
    auto stmt = parseStatement();
    if (!stmt)
      break;
    last = std::move(*stmt);
    any = true;
    if (Cur.kind == TokenKind::Tok_RBrace || Cur.kind == TokenKind::Tok_EOF)
      break;
  }
  if (!any)
    return std::nullopt;
  return last;
}

std::optional<ParseResult> Parser::parseStatement()
{
  // std::cout << "[parseStatement] Cur.kind=" << (int)Cur.kind << " text=" << Cur.text << std::endl;
  // std::cerr << "DEBUG: enter parseStatement Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // Try block
  if (auto b = parseBlock())
    return b;
  // variable statement
  if (auto vs = parseVariableStatement())
    return vs;
  // class declaration
  if (auto cd = parseClassDeclaration())
    return cd;
  // empty statement
  if (auto e = parseEmptyStatement())
    return e;
  // import/export/print
  if (auto imp = parseImportStatement())
    return imp;
  if (auto exp = parseExportStatement())
    return exp;
  if (auto p = parsePrintStatement())
    return p;
  // expression statement
  // labelled statement (Identifier ':' statement) should be tried before expression statements
  if (auto ls = parseLabelledStatement())
    return ls;
  // expression statement
  if (auto es = parseExpressionStatement())
    return es;
  // if statement
  if (auto iff = parseIfStatement())
    return iff;
  // iteration statements
  if (auto it = parseIterationStatement())
    return it;
  // continue/break/return/yield
  if (auto c = parseContinueStatement())
    return c;
  if (auto b = parseBreakStatement())
    return b;
  if (auto r = parseReturnStatement())
    return r;
  if (auto y = parseYieldStatement())
    return y;
  // with statement
  if (auto w = parseWithStatement())
    return w;
  if (auto sw = parseSwitchStatement())
    return sw;
  // std::cerr << "DEBUG: exit parseStatement no match Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return std::nullopt;
}

std::optional<ParseResult> Parser::handleOptionalHashBang()
{
  if (Cur.kind != TokenKind::Tok_Hashtag)
    return std::nullopt;
  advance();
  if (Cur.kind == TokenKind::Tok_EOF)
    return ParseResult{true, std::string(), nullptr};
  return std::nullopt;
}

std::optional<ParseResult> Parser::parseEmptyStatement()
{
  // emptyStatement_ : SemiColon
  // Only accept an explicit semicolon here. Using parseEos() would allow
  // implicit semicolon insertion (line terminators) to satisfy an empty
  // statement without consuming any token which can cause the parser loop
  // to make no progress. Require a real ';' token and consume it.
  if (Cur.kind != TokenKind::Tok_Semi)
    return std::nullopt;
  advance();
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseExpressionSequence()
{
  // expressionSequence : singleExpression (',' singleExpression)*
  if (!parseSingleExpression())
    return false;
  while (Cur.kind == TokenKind::Tok_Comma)
  {
    advance();
    if (!parseSingleExpression())
      return false;
  }
  return true;
}

std::optional<ParseResult> Parser::parseExpressionStatement()
{
  // Try declaration first if current token is Class/Function/Var/Let/Const
  switch (Cur.kind)
  {
  case TokenKind::Tok_Class:
  case TokenKind::Tok_Function:
  case TokenKind::Tok_Var:
  case TokenKind::Tok_NonStrictLet:
  case TokenKind::Tok_StrictLet:
  case TokenKind::Tok_Const:
    if (auto decl = parseDeclaration())
      return decl;
    break;
  default:
    break;
  }
  // Guard from grammar: {this.notOpenBraceAndNotFunction()}? expressionSequence eos
  // We'll conservatively assume the guard passes when current token is not '{' and not 'function' keyword
  if (Cur.kind == TokenKind::Tok_LBrace || Cur.kind == TokenKind::Tok_Function)
    return std::nullopt;
  auto save = Cur;
  if (!parseExpressionSequence())
  {
    Cur = save;
    return std::nullopt;
  }
  parseEos();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseIfStatement()
{
  // ifStatement : If '(' expressionSequence ')' statement (Else statement)?
  if (Cur.kind != TokenKind::Tok_If)
    return std::nullopt;
  advance();
  if (Cur.kind != TokenKind::Tok_LParen)
    return error("expected '(' after if");
  advance();
  if (!parseExpressionSequence())
    return error("invalid if condition");
  if (Cur.kind != TokenKind::Tok_RParen)
    return error("expected ')' after if condition");
  advance();
  // parse the statement after the if
  if (auto stmt = parseStatement())
  {
    // optional else
    if (Cur.kind == TokenKind::Tok_Else)
    {
      advance();
      if (!parseStatement())
        return error("expected statement after else");
    }
    return std::move(*stmt);
  }
  return error("invalid if body");
}

std::optional<ParseResult> Parser::parseIterationStatement()
{
  // iterationStatement covers Do..While, While, For, ForIn, ForOf
  auto save = Cur;
  if (Cur.kind == TokenKind::Tok_Do)
  {
    advance();
    // body
    if (!parseStatement())
      return error("expected statement after do");
    if (Cur.kind != TokenKind::Tok_While)
      return error("expected 'while' after do statement");
    advance();
    if (Cur.kind != TokenKind::Tok_LParen)
      return error("expected '(' after while");
    advance();
    if (!parseExpressionSequence())
      return error("invalid while condition");
    if (Cur.kind != TokenKind::Tok_RParen)
      return error("expected ')' after while condition");
    advance();
    // expect eos
    parseEos();
    return ParseResult{true, std::string(), nullptr};
  }

  if (Cur.kind == TokenKind::Tok_While)
  {
    advance();
    if (Cur.kind != TokenKind::Tok_LParen)
      return error("expected '(' after while");
    advance();
    if (!parseExpressionSequence())
      return error("invalid while condition");
    if (Cur.kind != TokenKind::Tok_RParen)
      return error("expected ')' after while condition");
    advance();
    if (auto stmt = parseStatement())
      return std::move(*stmt);
    return error("invalid while body");
  }

  if (Cur.kind == TokenKind::Tok_For)
  {
    advance();
    bool isAwait = false;
    // For Await? '(' ... ')'
    if (Cur.kind == TokenKind::Tok_Await)
    {
      isAwait = true;
      advance();
    }
    if (Cur.kind != TokenKind::Tok_LParen)
      return error("expected '(' after for");
    advance();
    // Try (expressionSequence | variableDeclarationList)?
    if (Cur.kind != TokenKind::Tok_Semi)
    {
      auto save2 = Cur;
      if (!parseExpressionSequence())
      {
        Cur = save2;
        if (!parseVariableDeclarationList())
        {
          Cur = save;
          return error("invalid for initializer");
        }
      }
    }
    if (!parseEos())
      return error("expected ';' in for");
    // optional expressionSequence
    if (Cur.kind != TokenKind::Tok_Semi)
    {
      if (!parseExpressionSequence())
        return error("invalid for condition");
    }
    if (!parseEos())
      return error("expected second ';' in for");
    // optional expressionSequence
    if (Cur.kind != TokenKind::Tok_RParen)
    {
      if (!parseExpressionSequence())
        return error("invalid for increment");
    }
    if (Cur.kind != TokenKind::Tok_RParen)
      return error("expected ')' after for");
    advance();
    if (auto stmt = parseStatement())
      return std::move(*stmt);
    return error("invalid for body");
  }

  // ForIn: For '(' (singleExpression | variableDeclarationList) In expressionSequence ')' statement
  // ForOf is similar with 'Of' and optional Await handled earlier
  // Attempt to parse ForIn/ForOf by peeking
  Cur = save;
  return std::nullopt;
}

std::optional<ParseResult> Parser::parseContinueStatement()
{
  // Continue ({this.notLineTerminator()}? identifier)? eos
  if (Cur.kind != TokenKind::Tok_Continue)
    return std::nullopt;
  advance();
  // optional identifier (we allow identifier-like tokens)
  if (!parseEos())
  {
    parseIdentifierName();
  }
  parseEos();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseBreakStatement()
{
  // Break ({this.notLineTerminator()}? identifier)? eos
  if (Cur.kind != TokenKind::Tok_Break)
    return std::nullopt;
  // capture break token position
  Token breakTok = Cur;
  advance();
  // Only accept an optional identifier if there is no line terminator between
  // the end of the 'break' token and the start of the following token.
  if (!parseEos())
  {
    size_t from = breakTok.pos + breakTok.text.size();
    size_t to = Cur.pos;
    if (!L.ContainsLineTerminatorBetween(from, to))
    {
      parseIdentifierName();
    }
  }
  parseEos();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseReturnStatement()
{
  // Return ({this.notLineTerminator()}? expressionSequence)? eos
  if (Cur.kind != TokenKind::Tok_Return)
    return std::nullopt;
  // capture return token position
  Token returnTok = Cur;
  advance();
  // Only attempt to parse an expressionSequence if there is no line terminator between
  // the end of the 'return' token and the start of the next token.
  if (!parseEos())
  {
    size_t from = returnTok.pos + returnTok.text.size();
    size_t to = Cur.pos;
    if (!L.ContainsLineTerminatorBetween(from, to))
    {
      // attempt to parse expressionSequence (conservative; ignore failure)
      parseExpressionSequence();
    }
  }
  parseEos();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseYieldStatement()
{
  // (Yield | YieldStar) ({this.notLineTerminator()}? expressionSequence)? eos
  if (Cur.kind != TokenKind::Tok_Yield && Cur.kind != TokenKind::Tok_YieldStar)
    return std::nullopt;
  // capture yield token
  Token yieldTok = Cur;
  advance();
  // Only parse the optional expressionSequence if there's no line terminator between
  // the yield token and the following token.
  if (!parseEos())
  {
    size_t from = yieldTok.pos + yieldTok.text.size();
    size_t to = Cur.pos;
    if (!L.ContainsLineTerminatorBetween(from, to))
    {
      parseExpressionSequence();
    }
  }
  parseEos();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseWithStatement()
{
  // withStatement : With '(' expressionSequence ')' statement
  if (Cur.kind != TokenKind::Tok_With)
    return std::nullopt;
  advance();
  if (Cur.kind != TokenKind::Tok_LParen)
    return error("expected '(' after with");
  advance();
  if (!parseExpressionSequence())
    return error("invalid with expression");
  if (Cur.kind != TokenKind::Tok_RParen)
    return error("expected ')' after with expression");
  advance();
  if (auto stmt = parseStatement())
    return std::move(*stmt);
  return error("invalid with body");
}

std::optional<ParseResult> Parser::parseLabelledStatement()
{
  // labelledStatement
  //   : Identifier ':' statement
  // Conservative implementation using existing helpers: try to consume an identifier-like token
  auto saved = Cur;
  if (!parseIdentifierName())
    return std::nullopt;
  if (Cur.kind != TokenKind::Tok_Colon)
  {
    // restore conservative state (note: lexer state isn't restored; follows repo pattern)
    Cur = saved;
    return std::nullopt;
  }
  // consume ':'
  advance();
  auto inner = parseStatement();
  if (!inner)
    return error("invalid labelled statement body");
  return std::move(*inner);
}

std::optional<ParseResult> Parser::parseDeclaration()
{
  // declaration : variableStatement | classDeclaration | functionDeclaration
  // For now we implement a conservative recognition:
  // - variableStatement: starts with Var/let_/Const
  // - classDeclaration: starts with Class
  // - functionDeclaration: starts with Function_
  switch (Cur.kind)
  {
  case TokenKind::Tok_Var:
  case TokenKind::Tok_NonStrictLet:
  case TokenKind::Tok_StrictLet:
  case TokenKind::Tok_Const:
  {
    // variableStatement -> variableDeclarationList eos
    // Consume the var/let/const token and then accept until eos conservatively.
    advance();
    // consume tokens until we hit eos or parser can't progress.
    while (!parseEos())
    {
      // rudimentary consumption: stop on tokens that likely start a new statement/block
      if (Cur.kind == TokenKind::Tok_LBrace || Cur.kind == TokenKind::Tok_RBrace)
        break;
      advance();
    }
    parseEos();
    return ParseResult{true, std::string(), nullptr};
  }
  case TokenKind::Tok_Class:
  {
    // delegate to parseClassDeclaration
    if (auto cd = parseClassDeclaration())
      return cd;
    return std::nullopt;
  }
  case TokenKind::Tok_Function:
  {
    // functionDeclaration : Async? Function identifier '(' params ')' '{' body '}'
    if (Cur.kind == TokenKind::Tok_Async)
      advance();
    if (Cur.kind != TokenKind::Tok_Function)
      return std::nullopt;
    advance();
    // optional identifier
    // Accept optional identifier, but don't require it
    if (Cur.kind == TokenKind::Tok_Identifier)
    {
      parseIdentifierName();
    }
    if (Cur.kind != TokenKind::Tok_LParen)
      return error("expected '(' after function declaration");
    advance();
    // Consume parameters until ')'
    int parenDepth = 1;
    while (Cur.kind != TokenKind::Tok_EOF && parenDepth > 0)
    {
      if (Cur.kind == TokenKind::Tok_LParen)
      {
        parenDepth++;
        advance();
        continue;
      }
      if (Cur.kind == TokenKind::Tok_RParen)
      {
        parenDepth--;
        advance();
        continue;
      }
      advance();
    }
    // Expect '{' for function body
    if (Cur.kind != TokenKind::Tok_LBrace)
      return error("expected '{' after function parameter list");
    // Consume function body until matching '}'
    int braceDepth = 1;
    advance();
    while (Cur.kind != TokenKind::Tok_EOF && braceDepth > 0)
    {
      if (Cur.kind == TokenKind::Tok_LBrace)
        braceDepth++;
      else if (Cur.kind == TokenKind::Tok_RBrace)
        braceDepth--;
      advance();
    }
    return ParseResult{true, std::string(), nullptr};
  }
  default:
    return std::nullopt;
  }
}

std::optional<ParseResult> Parser::parseThrowStatement()
{
  // throwStatement : Throw {this.notLineTerminator()}? expressionSequence eos
  if (Cur.kind != TokenKind::Tok_Print && Cur.kind != TokenKind::Tok_ConsoleLog)
    return std::nullopt;
  advance();
  if (Cur.kind != TokenKind::Tok_LParen)
    return error("expected '('");
  advance();
  // Accept any tokens until matching ')'
  int parenDepth = 1;
  while (Cur.kind != TokenKind::Tok_EOF && parenDepth > 0)
  {
    if (Cur.kind == TokenKind::Tok_LParen)
    {
      parenDepth++;
    }
    else if (Cur.kind == TokenKind::Tok_RParen)
    {
      parenDepth--;
    }
    advance();
  }
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseTryStatement()
{
  // tryStatement : Try block (catchProduction finallyProduction? | finallyProduction)
  if (Cur.kind != TokenKind::Tok_Try)
    return std::nullopt;
  advance();
  // require a block
  if (Cur.kind != TokenKind::Tok_LBrace)
    return error("expected '{' after try");
  // parse block (we already have parseBlock that returns ParseResult)
  if (!parseBlock())
    return error("invalid try block");
  // Now either catchProduction (with optional finally) or finallyProduction
  if (Cur.kind == TokenKind::Tok_Catch)
  {
    if (!parseCatchProduction())
      return error("invalid catch clause");
    // optional finally
    if (Cur.kind == TokenKind::Tok_Finally)
    {
      if (!parseFinallyProduction())
        return error("invalid finally clause");
    }
    return ParseResult{true, std::string(), nullptr};
  }
  if (Cur.kind == TokenKind::Tok_Finally)
  {
    if (!parseFinallyProduction())
      return error("invalid finally clause");
    return ParseResult{true, std::string(), nullptr};
  }
  return error("expected 'catch' or 'finally' after try block");
}

bool Parser::parseCatchProduction()
{
  // catchProduction : Catch ('(' assignable? ')')? block
  if (Cur.kind != TokenKind::Tok_Catch)
    return false;
  advance();
  if (Cur.kind == TokenKind::Tok_LParen)
  {
    advance();
    // optional assignable
    if (Cur.kind != TokenKind::Tok_RParen)
    {
      if (!parseAssignable())
        return false;
    }
    if (Cur.kind != TokenKind::Tok_RParen)
      return false;
    advance();
  }
  // require block
  if (Cur.kind != TokenKind::Tok_LBrace)
    return false;
  // parse block (conservatively)
  if (!parseBlock())
    return false;
  return true;
}

bool Parser::parseFinallyProduction()
{
  // finallyProduction : Finally block
  if (Cur.kind != TokenKind::Tok_Finally)
    return false;
  advance();
  // parseBlock validates and consumes the opening '{' and the block contents.
  if (!parseBlock())
    return false;
  return true;
}

std::optional<ParseResult> Parser::parseDebuggerStatement()
{
  // debuggerStatement : Debugger eos
  if (Cur.kind != TokenKind::Tok_Debugger)
    return std::nullopt;
  advance();
  parseEos();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseFunctionDeclaration()
{
  // functionDeclaration
  //   : Async? Function_ '*'? identifier '(' formalParameterList? ')' functionBody
  auto save = Cur;
  // optional Async
  if (Cur.kind == TokenKind::Tok_Async)
    advance();
  if (Cur.kind != TokenKind::Tok_Function)
  {
    Cur = save;
    return std::nullopt;
  }
  advance();
  // optional '*' (not tokenized specially here; accept a Multiply token or skip if absent)
  if (Cur.kind == TokenKind::Tok_Multiply)
    advance();
  // required identifier (function name)
  if (!parseIdentifierName())
  {
    return error("expected function name after 'function' keyword");
  }
  // expect '(' for parameter list
  if (Cur.kind != TokenKind::Tok_LParen)
    return error("expected '(' after function declaration");
  advance();
  // consume parameter list tokens until matching ')'
  int depth = 1;
  while (Cur.kind != TokenKind::Tok_EOF && depth > 0)
  {
    if (Cur.kind == TokenKind::Tok_LParen)
    {
      depth++;
      advance();
      continue;
    }
    if (Cur.kind == TokenKind::Tok_RParen)
    {
      depth--;
      advance();
      continue;
    }
    advance();
  }
  if (depth != 0)
    return error("expected ')' after function parameter list");
  // expect '{' for function body
  if (Cur.kind != TokenKind::Tok_LBrace)
    return error("expected '{' after function parameter list");
  // consume function body until matching '}'
  int braceDepth = 1;
  advance();
  while (Cur.kind != TokenKind::Tok_EOF && braceDepth > 0)
  {
    if (Cur.kind == TokenKind::Tok_LBrace)
    {
      braceDepth++;
      advance();
      continue;
    }
    if (Cur.kind == TokenKind::Tok_RBrace)
    {
      braceDepth--;
      advance();
      continue;
    }
    advance();
  }
  if (braceDepth != 0)
    return error("expected '}' after function body");
  // For now, just return success
  return ParseResult{true, std::string(), nullptr};
  while (Cur.kind != TokenKind::Tok_EOF)
  {
    if (Cur.kind == TokenKind::Tok_LParen)
      depth++;
    else if (Cur.kind == TokenKind::Tok_RParen)
    {
      depth--;
      advance();
      if (depth == 0)
        break;
      continue;
    }
    advance();
  }
  // now functionBody
  if (!parseFunctionBody())
    return error("invalid function body");
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseClassDeclaration()
{
  // std::cerr << "DEBUG: enter parseClassDeclaration Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // classDeclaration : Class identifier classTail
  if (Cur.kind != TokenKind::Tok_Class)
    return std::nullopt;
  advance();
  // std::cerr << "DEBUG: after Class advance Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // require identifier (class name) if present: use parseIdentifierName
  if (parseIdentifierName())
  {
    // std::cerr << "DEBUG: after class name advance Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  }
  // std::cerr << "DEBUG: after parseIdentifierName Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // Robustly advance to '{' after class name
  if (Cur.kind != TokenKind::Tok_LBrace)
  {
    std::cerr << "WARNING: Forcibly advancing to '{' after class name. Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    while (Cur.kind != TokenKind::Tok_LBrace && Cur.kind != TokenKind::Tok_EOF)
    {
      advance();
      std::cerr << "TRACE: Advancing to '{'... Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    }
  }
  if (Cur.kind == TokenKind::Tok_LBrace)
  {
    if (!parseClassTail())
      return error("invalid class tail");
  }
  else
  {
    std::cerr << "ERROR: Expected '{' after class name, got Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    return error("expected '{' after class name");
  }
  // std::cerr << "DEBUG: after parseClassTail Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // After class body, forcibly advance until RBrace (closing class)
  while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF)
  {
    std::cerr << "WARNING: parseClassDeclaration forcibly advancing after class body Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    advance();
  }
  // If at RBrace, advance once to move past class
  if (Cur.kind == TokenKind::Tok_RBrace)
  {
    advance();
    // std::cerr << "DEBUG: after advancing past RBrace Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  }
  // Consume trailing semicolon or EOS after class declaration
  parseEos();
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseClassTail()
{
  // std::cerr << "DEBUG: enter parseClassTail Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // classTail : (Extends singleExpression)? '{' classElement* '}'
  if (Cur.kind == TokenKind::Tok_Extends)
  {
    advance();
    if (!parseSingleExpression())
      return false;
  }
  if (Cur.kind != TokenKind::Tok_LBrace)
    return false;
  advance();
  // std::cerr << "DEBUG: after LBrace advance Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // zero or more classElement
  int elemCount = 0;
  // Consume tokens until closing brace is found
  while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF)
  {
    // std::cerr << "DEBUG: parseClassTail loop elem=" << elemCount << " Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    Token before = Cur;
    bool consumed = parseClassElement();
    std::cerr << "TRACE: After parseClassElement, Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << " (before.pos=" << before.pos << ")\n";
    if (!consumed || (Cur.pos == before.pos && Cur.kind == before.kind))
    {
      std::cerr << "TRACE: parseClassTail did NOT consume token, forcibly advancing. Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
      advance();
    }
    ++elemCount;
  }
  if (Cur.kind != TokenKind::Tok_RBrace)
  {
    std::cerr << "ERROR: parseClassTail did not find closing RBrace, Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    return false;
  }
  // Do not advance after closing brace; let parseClassDeclaration handle it
  // std::cerr << "DEBUG: at RBrace Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  return true;
}

bool Parser::parseClassElement()
{
  // Always try to parse a class element name (static, get, set, identifier, etc.)
  // std::cerr << "DEBUG: parseClassElement entry Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  Token startElem = Cur;
  if (!parseClassElementName())
  {
    std::cerr << "WARNING: Unrecognized class member, forcibly advancing token to avoid stall. Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    advance();
    std::cerr << "TRACE: parseClassElementName did NOT consume token, forcibly advanced. Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    return true;
  }
  // std::cerr << "DEBUG: after parseClassElementName Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  std::cerr << "TRACE: parseClassElementName consumed token(s), now Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  std::cerr << "TRACE: parseClassElement startElem.pos=" << startElem.pos << " endElem.pos=" << Cur.pos << "\n";

  // Field assignment: '=' singleExpression ';'?
  if (Cur.kind == TokenKind::Tok_Assign)
  {
    advance();
    // Consume everything until semicolon, RBrace, or EOF
    while (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF && Cur.kind != TokenKind::Tok_RBrace)
    {
      advance();
    }
    if (Cur.kind == TokenKind::Tok_Semi)
      advance();
    return true;
  }

  // Method or getter/setter: '(' ... ')' '{' ... '}'
  if (Cur.kind == TokenKind::Tok_LParen)
  {
    // Consume params until ')'
    int depth = 0;
    while (Cur.kind != TokenKind::Tok_EOF)
    {
      std::cout << "[Parser] Top-level token kind: " << (int)Cur.kind << " text: " << Cur.text << std::endl;
      if (Cur.kind == TokenKind::Tok_LParen)
        depth++;
      if (Cur.kind == TokenKind::Tok_RParen)
      {
        depth--;
        advance();
        if (depth == 0)
          break;
        continue;
      }
      advance();
    }
    // After params, expect method body '{' ... '}'
    if (Cur.kind == TokenKind::Tok_LBrace)
    {
      int braceDepth = 0;
      while (Cur.kind != TokenKind::Tok_EOF)
      {
        if (Cur.kind == TokenKind::Tok_LBrace)
          braceDepth++;
        if (Cur.kind == TokenKind::Tok_RBrace)
        {
          braceDepth--;
          advance();
          if (braceDepth == 0)
            break;
          continue;
        }
        advance();
      }
    }
    return true;
  }

  // Block (for static blocks): '{' ... '}'
  if (Cur.kind == TokenKind::Tok_LBrace)
  {
    int braceDepth = 0;
    while (Cur.kind != TokenKind::Tok_EOF)
    {
      if (Cur.kind == TokenKind::Tok_LBrace)
        braceDepth++;
      if (Cur.kind == TokenKind::Tok_RBrace)
      {
        braceDepth--;
        advance();
        if (braceDepth == 0)
          break;
        continue;
      }
      advance();
    }
    return true;
  }

  // Semicolon (empty member)
  if (Cur.kind == TokenKind::Tok_Semi)
  {
    advance();
    return true;
  }

  // If nothing matched, forcibly advance until Tok_RBrace, Tok_EOF, or Tok_Semi
  std::cerr << "WARNING: parseClassElement did not match any recognizer, forcibly advancing until Tok_RBrace, Tok_EOF, or Tok_Semi. Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF && Cur.kind != TokenKind::Tok_Semi)
  {
    advance();
  }
  if (Cur.kind == TokenKind::Tok_Semi)
    advance();
  return true;
}

std::optional<ParseResult> Parser::parseSourceElements()
{
  // sourceElements : sourceElement*
  std::vector<std::unique_ptr<Stmt>> stmts;
  int iter = 0;
  while (Cur.kind != TokenKind::Tok_EOF)
  {
    Token before = Cur;
    bool advanced = false;
    std::unique_ptr<Stmt> stmt;
    // Top-level class declaration
    if (Cur.kind == TokenKind::Tok_Class)
    {
      auto s = parseClassDeclaration();
      if (!s)
        break;
      stmt = std::move(s->stmt);
      advanced = true;
    }
    // Top-level function declaration
    else if (Cur.kind == TokenKind::Tok_Function)
    {
      auto s = parseFunctionDeclaration();
      if (!s)
        break;
      stmt = std::move(s->stmt);
      advanced = true;
    }
    // Top-level variable declaration (const, var)
    else if (Cur.kind == TokenKind::Tok_Const || Cur.kind == TokenKind::Tok_Var)
    {
      bool ok = parseVariableDeclaration();
      if (!ok)
        break;
      advanced = true;
    }
    // Top-level import/export/print
    else if (Cur.kind == TokenKind::Tok_Import || Cur.kind == TokenKind::Tok_Export || Cur.kind == TokenKind::Tok_Print)
    {
      auto s = parseStatement();
      if (!s)
        break;
      stmt = std::move(s->stmt);
      advanced = true;
      // If the parsed statement is a PrintStmt, call parseEos()
      if (stmt && dynamic_cast<PrintStmt*>(stmt.get())) {
        parseEos();
      }
    }
    else if (Cur.kind == TokenKind::Tok_ConsoleLog)
    {
      auto s = parsePrintStatement();
      if (!s)
        break;
      stmt = std::move(s->stmt);
      advanced = true;
      parseEos();
    }
    else if (Cur.kind == TokenKind::Tok_ConsoleError)
    {
      auto s = parsePrintStatement();
      if (!s)
        break;
      stmt = std::move(s->stmt);
      advanced = true;
      parseEos();
    }
    else if (Cur.kind == TokenKind::Tok_ConsoleWarn)
    {
      auto s = parsePrintStatement();
      if (!s)
        break;
      stmt = std::move(s->stmt);
      advanced = true;
      parseEos();
    }
    else if (Cur.kind == TokenKind::Tok_ConsoleInfo)
    {
      auto s = parsePrintStatement();
      if (!s)
        break;
      stmt = std::move(s->stmt);
      advanced = true;
      parseEos();
    }
    else if (Cur.kind == TokenKind::Tok_ConsoleSuccess) {
      auto s = parsePrintStatement();
      if (!s)
        break;
      stmt = std::move(s->stmt);
      advanced = true;
      parseEos();
    }
    // Top-level expression statement (e.g., assignments, calls)
    else if (Cur.kind == TokenKind::Tok_Identifier || Cur.kind == TokenKind::Tok_LParen || Cur.kind == TokenKind::Tok_Number || Cur.kind == TokenKind::Tok_StringLiteral)
    {
      auto s = parseExpressionStatement();
      if (s)
      {
        stmt = std::move(s->stmt);
        advanced = true;
      }
      else
      {
        advance();
        advanced = true;
      }
    }
    else
    {
      advance();
      advanced = true;
    }
    if (stmt)
      stmts.push_back(std::move(stmt));
    if (!advanced || (Cur.pos == before.pos && Cur.kind == before.kind))
    {
      advance();
      std::cerr << "WARNING: forcibly advanced token in parseSourceElements loop Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    }
    ++iter;
  }
  if (stmts.empty())
    return std::nullopt;
  auto prog = std::make_unique<Program>(std::move(stmts));
  return ParseResult{true, std::string(), std::move(prog)};
}

bool Parser::parseMethodDefinition()
{
  // methodDefinition: conservative approach
  // try to find '(' followed by ')' then a function body '{'
  auto save = Cur;
  // optional Async with optional notLineTerminator predicate (conservative)
  if (Cur.kind == TokenKind::Tok_Async)
  {
    Token asyncTok = Cur;
    advance();
    // we don't strictly enforce the notLineTerminator predicate here; if required later
    // we can call L.ContainsLineTerminatorBetween(asyncTok.pos + asyncTok.text.size(), Cur.pos)
    // and reject when it's true. For now accept Async optimistically.
  }
  // optional '*' for generator methods
  if (Cur.kind == TokenKind::Tok_Multiply)
    advance();
  // scan forward for '('
  while (Cur.kind != TokenKind::Tok_EOF && Cur.kind != TokenKind::Tok_LParen && Cur.kind != TokenKind::Tok_RBrace)
    advance();
  if (Cur.kind != TokenKind::Tok_LParen)
  {
    Cur = save;
    return false;
  }
  // consume parameters quickly until matching ')'
  int depth = 0;
  while (Cur.kind != TokenKind::Tok_EOF)
  {
    if (Cur.kind == TokenKind::Tok_LParen)
      depth++;
    else if (Cur.kind == TokenKind::Tok_RParen)
    {
      depth--;
      advance();
      if (depth == 0)
        break;
      continue;
    }
    advance();
  }
  // expect function body
  if (!parseFunctionBody())
  {
    Cur = save;
    return false;
  }
  return true;
}

bool Parser::parseFunctionBody()
{
  // functionBody : '{' sourceElements? '}'
  // Reuse parseBlock which returns an optional ParseResult; convert to bool.
  auto save = Cur;
  if (!parseBlock())
  {
    Cur = save;
    return false;
  }
  return true;
}

bool Parser::parseFieldDefinition()
{
  // fieldDefinition : classElementName initializer?
  auto save = Cur;
  // accept a classElementName
  if (!parseClassElementName())
  {
    Cur = save;
    return false;
  }
  // optional initializer
  if (Cur.kind == TokenKind::Tok_Assign)
  {
    advance();
    if (!parseSingleExpression())
    {
      Cur = save;
      return false;
    }
  }
  // optional semicolon / eos
  parseEos();
  return true;
}

bool Parser::parseClassElementName()
{
  // classElementName : propertyName | privateIdentifier
  // propertyName : identifierName | StringLiteral | numericLiteral | '[' singleExpression ']'
  // std::cerr << "DEBUG: parseClassElementName Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  // Always advance for identifier-like tokens (maximal robustness)
  if (Cur.kind == TokenKind::Tok_Identifier || Cur.kind == TokenKind::Tok_Static || Cur.kind == TokenKind::Tok_Async || Cur.text == "get" || Cur.text == "set" || Cur.text == "constructor" || Cur.kind == TokenKind::Tok_Private || Cur.kind == TokenKind::Tok_Public || Cur.kind == TokenKind::Tok_Protected)
  {
    // std::cerr << "DEBUG: parseClassElementName advancing identifier-like Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    advance();
    return true;
  }
  if (Cur.kind == TokenKind::Tok_StringLiteral || Cur.kind == TokenKind::Tok_DecimalLiteral || Cur.kind == TokenKind::Tok_Integer)
  {
    // std::cerr << "DEBUG: parseClassElementName advancing literal Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    advance();
    return true;
  }
  if (Cur.kind == TokenKind::Tok_LBracket)
  {
    // std::cerr << "DEBUG: parseClassElementName entering computed property Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    advance();
    if (!parseSingleExpression())
      return false;
    if (Cur.kind != TokenKind::Tok_RBracket)
      return false;
    advance();
    return true;
  }
  if (Cur.kind == TokenKind::Tok_Hashtag)
  {
    // std::cerr << "DEBUG: parseClassElementName entering private identifier Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
    return parsePrivateIdentifier();
  }
  // If nothing matches, forcibly advance and return true
  std::cerr << "WARNING: parseClassElementName did not match any recognizer, forcibly advancing token Cur.kind=" << (int)Cur.kind << " text='" << Cur.text << "' pos=" << Cur.pos << "\n";
  advance();
  return true;
}

bool Parser::parsePrivateIdentifier()
{
  // privateIdentifier : '#' identifierName
  if (Cur.kind != TokenKind::Tok_Hashtag)
    return false;
  advance();
  if (!parseIdentifierName())
    return false;
  return true;
}

bool Parser::parseFormalParameterList()
{
  // formalParameterList
  //   : formalParameterArg (',' formalParameterArg)* (',' lastFormalParameterArg)?
  //   | lastFormalParameterArg
  // Conservative implementation: parse formalParameterArg (assignable ('=' singleExpression)?)
  // or lastFormalParameterArg (Ellipsis singleExpression)
  auto save = Cur;
  if (parseLastFormalParameterArg())
    return true;
  // otherwise must start with a formalParameterArg
  if (!parseFormalParameterArg())
  {
    Cur = save;
    return false;
  }
  while (Cur.kind == TokenKind::Tok_Comma)
  {
    advance();
    // if the next is a rest parameter, accept and finish
    if (parseLastFormalParameterArg())
      return true;
    if (!parseFormalParameterArg())
    {
      Cur = save;
      return false;
    }
  }
  return true;
}

bool Parser::parseFormalParameterArg()
{
  // formalParameterArg : assignable ('=' singleExpression)?
  if (!parseAssignable())
    return false;
  if (Cur.kind == TokenKind::Tok_Assign)
  {
    advance();
    if (!parseSingleExpression())
      return false;
  }
  return true;
}

bool Parser::parseLastFormalParameterArg()
{
  // lastFormalParameterArg : Ellipsis singleExpression
  if (Cur.kind != TokenKind::Tok_Ellipsis)
    return false;
  advance();
  if (!parseSingleExpression())
    return false;
  return true;
}

std::optional<ParseResult> Parser::parseVariableStatement()
{
  // variableStatement : variableDeclarationList eos
  auto save = Cur;
  if (!parseVariableDeclarationList())
  {
    Cur = save;
    return std::nullopt;
  }
  // accept semicolon or EOF as eos
  parseEos();
  // If the last variable declaration included a type annotation, print it
  if (LastTypeParsed)
  {
    // std::cerr << "DEBUG: parsed type annotation: " << typeToString(LastTypeParsed.get()) << "\n";
    // clear it; variable declaration handling is currently per-declaration so
    // we don't attach types to AST nodes yet.
    LastTypeParsed.reset();
  }
  return ParseResult{true, std::string(), nullptr};
}

std::unique_ptr<Type> Parser::takeParsedType()
{
  return std::move(LastTypeParsed);
}

bool Parser::parseVariableDeclarationList()
{
  // variableDeclarationList : varModifier variableDeclaration (',' variableDeclaration)*
  if (!parseVarModifier())
    return false;
  if (!parseVariableDeclaration())
    return false;
  while (Cur.kind == TokenKind::Tok_Comma)
  {
    advance();
    if (!parseVariableDeclaration())
      return false;
  }
  return true;
}

bool Parser::parseVarModifier()
{
  // varModifier : Var | let_ | Const
  switch (Cur.kind)
  {
  case TokenKind::Tok_Var:
  case TokenKind::Tok_Const:
    advance();
    return true;
  case TokenKind::Tok_NonStrictLet:
  case TokenKind::Tok_StrictLet:
    // delegate to parseLet_ for clarity
    return parseLet_();
  default:
    return false;
  }
}

bool Parser::parseLet_()
{
  // let_ : NonStrictLet | StrictLet
  switch (Cur.kind)
  {
  case TokenKind::Tok_NonStrictLet:
  case TokenKind::Tok_StrictLet:
    advance();
    return true;
  default:
    return false;
  }
}

bool Parser::parseVariableDeclaration()
{
  // variableDeclaration : assignable ('=' singleExpression)?
  if (!parseAssignable())
    return false;
  // optional TypeScript type annotation
  if (Cur.kind == TokenKind::Tok_Colon)
  {
    advance();
    if (!parseType())
      return false;
  }
  if (Cur.kind == TokenKind::Tok_Assign)
  {
    advance();
    if (!parseSingleExpression())
      return false;
  }
  return true;
}

bool Parser::parseType()
{
  // Conservative Type parser supporting:
  //  - primary types: identifier or builtin type tokens
  //  - generics: Identifier '<' type (',' type)* '>'
  //  - parenthesized types: '(' type ')'
  //  - union and intersection: type ('|' | '&') type
  //  - array suffix '[]'

  // We'll build a Type AST in LastTypeParsed. Use local lambdas that return
  // std::unique_ptr<Type> for subcomponents and nullptr on failure.
  std::function<std::unique_ptr<Type>()> parsePrimary = [&]() -> std::unique_ptr<Type>
  {
    if (Cur.kind == TokenKind::Tok_LParen)
    {
      advance();
      // reuse outer parseType by recursive call: parseType will populate LastTypeParsed
      if (!parseType())
        return nullptr; // parseType will populate LastTypeParsed
      auto t = takeParsedType();
      if (!t)
        return nullptr;
      if (Cur.kind != TokenKind::Tok_RParen)
        return nullptr;
      advance();
      return t;
    }
    if (Cur.kind == TokenKind::Tok_LBrace)
    {
      // conservatively capture raw contents until matching '}'
      size_t start = Cur.pos;
      int depth = 0;
      std::string raw;
      while (Cur.kind != TokenKind::Tok_EOF)
      {
        if (Cur.kind == TokenKind::Tok_LBrace)
          depth++;
        if (Cur.kind == TokenKind::Tok_RBrace)
        {
          depth--;
          // include '}' and advance
          raw += Cur.text;
          advance();
          if (depth == 0)
            break;
          continue;
        }
        raw += Cur.text;
        advance();
      }
      return std::make_unique<RawType>(raw);
    }
    if (Cur.kind == TokenKind::Tok_LBracket)
    {
      // tuple/array start: capture raw until matching ']'
      std::string raw;
      int depth = 0;
      while (Cur.kind != TokenKind::Tok_EOF)
      {
        if (Cur.kind == TokenKind::Tok_LBracket)
          depth++;
        if (Cur.kind == TokenKind::Tok_RBracket)
        {
          depth--;
          raw += Cur.text;
          advance();
          if (depth == 0)
            break;
          continue;
        }
        raw += Cur.text;
        advance();
      }
      return std::make_unique<RawType>(raw);
    }

    // identifier-like or builtin type token
    switch (Cur.kind)
    {
    case TokenKind::Tok_Identifier:
    case TokenKind::Tok_Any:
    case TokenKind::Tok_Number:
    case TokenKind::Tok_Never:
    case TokenKind::Tok_Boolean:
    case TokenKind::Tok_String:
    case TokenKind::Tok_Unique:
    case TokenKind::Tok_Symbol:
    case TokenKind::Tok_Undefined:
    case TokenKind::Tok_Object:
    {
      // capture the name text
      std::string name = Cur.text;
      advance();
      std::unique_ptr<Type> base = std::make_unique<NamedType>(name);
      // generics: '<' ... '>' -- parse comma-separated types inside balancing '<' '>'
      if (Cur.kind == TokenKind::Tok_LessThan)
      {
        // consume '<'
        advance();
        auto gen = std::make_unique<GenericType>();
        gen->Base = std::move(base);
        // parse type args until matching '>'
        while (Cur.kind != TokenKind::Tok_EOF && Cur.kind != TokenKind::Tok_MoreThan)
        {
          // parse a type argument by recursively invoking parseType
          if (!parseType())
            return nullptr;
          auto arg = takeParsedType();
          if (!arg)
            return nullptr;
          gen->Args.push_back(std::move(arg));
          if (Cur.kind == TokenKind::Tok_Comma)
          {
            advance();
            continue;
          }
          // otherwise expect '>' or more tokens
        }
        if (Cur.kind == TokenKind::Tok_MoreThan)
          advance();
        base = std::move(gen);
      }
      // array suffixes '[]'
      while (Cur.kind == TokenKind::Tok_LBracket)
      {
        // expect ']'
        advance();
        if (Cur.kind == TokenKind::Tok_RBracket)
        {
          advance();
          base = std::make_unique<ArrayType>(std::move(base));
          continue;
        }
        break;
      }
      return base;
    }
    default:
      return nullptr;
    }
  };

  auto left = parsePrimary();
  if (!left)
    return false;

  // handle union '|' and intersection '&' operators
  while (Cur.kind == TokenKind::Tok_BitOr || Cur.kind == TokenKind::Tok_BitAnd)
  {
    bool isUnion = (Cur.kind == TokenKind::Tok_BitOr);
    advance();
    auto right = parsePrimary();
    if (!right)
      return false;
    if (isUnion)
    {
      auto u = std::make_unique<UnionType>();
      // flatten if left is already a union
      if (auto leftU = dynamic_cast<UnionType *>(left.get()))
      {
        for (auto &o : leftU->Options)
          u->Options.push_back(std::move(o));
      }
      else
      {
        u->Options.push_back(std::move(left));
      }
      u->Options.push_back(std::move(right));
      left = std::move(u);
    }
    else
    {
      auto it = std::make_unique<IntersectionType>();
      if (auto leftI = dynamic_cast<IntersectionType *>(left.get()))
      {
        for (auto &p : leftI->Parts)
          it->Parts.push_back(std::move(p));
      }
      else
      {
        it->Parts.push_back(std::move(left));
      }
      it->Parts.push_back(std::move(right));
      left = std::move(it);
    }
  }

  // store result
  LastTypeParsed = std::move(left);
  return true;
}

bool Parser::parseAssignable()
{
  // assignable : identifier | keyword | arrayLiteral | objectLiteral
  // Accept identifier-like tokens
  if (parseIdentifierName())
    return true;
  // object or array literal starts
  if (Cur.kind == TokenKind::Tok_LBrace)
  {
    return parseObjectLiteral();
  }
  if (Cur.kind == TokenKind::Tok_LBracket)
  {
    return parseArrayLiteral();
  }
  return false;
}

bool Parser::parseObjectLiteral()
{
  // objectLiteral : '{' (propertyAssignment (',' propertyAssignment)* ','?)? '}'
  if (Cur.kind != TokenKind::Tok_LBrace)
    return false;
  advance();
  // empty object
  if (Cur.kind == TokenKind::Tok_RBrace)
  {
    advance();
    return true;
  }
  // try propertyAssignment optionally separated by commas, allow trailing comma
  if (!parsePropertyAssignment())
  {
    // allow malformed but consume until '}' conservatively
    while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF)
      advance();
    if (Cur.kind == TokenKind::Tok_RBrace)
    {
      advance();
      return true;
    }
    return false;
  }
  while (Cur.kind == TokenKind::Tok_Comma)
  {
    advance();
    if (Cur.kind == TokenKind::Tok_RBrace)
      break; // trailing comma
    if (!parsePropertyAssignment())
    {
      // consume until '}' conservatively
      while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF)
        advance();
      break;
    }
  }
  if (Cur.kind != TokenKind::Tok_RBrace)
    return false;
  advance();
  return true;
}

bool Parser::parseAnonymousFunction()
{
  // anonymousFunction
  //   : functionDeclaration
  //   | Async? Function_ '*'? '(' formalParameterList? ')' functionBody
  //   | Async? arrowFunctionParameters '=>' arrowFunctionBody
  // Conservative recognizer: try functionDeclaration first, then anonymous function syntax, then arrow functions.
  auto save = Cur;
  // try a full functionDeclaration (may be named)
  if (auto fd = parseFunctionDeclaration())
    return true;
  Cur = save;

  // Async? Function_ '*'? '(' formalParameterList? ')' functionBody
  if (Cur.kind == TokenKind::Tok_Async)
  {
    advance();
  }
  if (Cur.kind == TokenKind::Tok_Function)
  {
    advance();
    if (Cur.kind == TokenKind::Tok_Multiply)
      advance();
    if (Cur.kind != TokenKind::Tok_LParen)
    {
      Cur = save;
      return false;
    }
    // consume parameter list quickly
    if (!parseArguments())
    {
      Cur = save;
      return false;
    }
    if (!parseFunctionBody())
    {
      Cur = save;
      return false;
    }
    return true;
  }
  Cur = save;

  // Async? arrowFunctionParameters '=>' arrowFunctionBody
  if (Cur.kind == TokenKind::Tok_Async)
  {
    advance();
  }
  if (parseArrowFunctionParameters())
  {
    // expect '=>' (lexer uses Tok_Arrow)
    if (Cur.kind != TokenKind::Tok_Arrow)
    {
      Cur = save;
      return false;
    }
    advance();
    // arrowFunctionBody: singleExpression | functionBody
    if (Cur.kind == TokenKind::Tok_LBrace)
    {
      if (!parseFunctionBody())
      {
        Cur = save;
        return false;
      }
    }
    else
    {
      if (!parseSingleExpression())
      {
        Cur = save;
        return false;
      }
    }
    return true;
  }

  Cur = save;
  return false;
}

bool Parser::parseArrayLiteral()
{
  // arrayLiteral : '[' elementList ']'
  if (Cur.kind != TokenKind::Tok_LBracket)
    return false;
  advance();
  // delegate to elementList
  if (!parseElementList())
    return false;
  if (Cur.kind != TokenKind::Tok_RBracket)
    return false;
  advance();
  return true;
}

bool Parser::parseElementList()
{
  // elementList : ','* arrayElement? (','+ arrayElement)* ','*
  // Conservative implementation: accept any number of commas and elements.
  // Leading commas represent elisions.
  while (Cur.kind == TokenKind::Tok_Comma)
  {
    advance();
  }
  if (parseArrayElement())
  {
    // after first element, zero or more groups of one-or-more commas + element
    while (Cur.kind == TokenKind::Tok_Comma)
    {
      // consume at least one comma
      int count = 0;
      while (Cur.kind == TokenKind::Tok_Comma)
      {
        advance();
        count++;
      }
      // require an element after comma(s) only if not trailing commas at end
      if (!parseArrayElement())
      {
        // allow trailing commas (elisions at end)
        return true;
      }
    }
  }
  return true;
}

bool Parser::parseArrayElement()
{
  // arrayElement : Ellipsis? singleExpression
  if (Cur.kind == TokenKind::Tok_Ellipsis)
  {
    advance();
    if (!parseSingleExpression())
      return false;
    return true;
  }
  // optional singleExpression
  if (parseSingleExpression())
    return true;
  return false;
}

bool Parser::parseArrowFunctionParameters()
{
  // arrowFunctionParameters : propertyName | '(' formalParameterList? ')'
  auto save = Cur;
  // parenthesized form
  if (Cur.kind == TokenKind::Tok_LParen)
  {
    // consume '(' and optional parameter list, reusing formal parameter parsing
    advance();
    if (Cur.kind != TokenKind::Tok_RParen)
    {
      if (!parseFormalParameterList())
      {
        Cur = save;
        return false;
      }
    }
    if (Cur.kind != TokenKind::Tok_RParen)
    {
      Cur = save;
      return false;
    }
    advance();
    return true;
  }

  // propertyName form - accept identifierName | StringLiteral | numericLiteral | '[' singleExpression ']'
  if (parsePropertyName())
    return true;

  Cur = save;
  return false;
}

bool Parser::parseArrowFunctionBody()
{
  // arrowFunctionBody : singleExpression | functionBody
  auto save = Cur;
  if (Cur.kind == TokenKind::Tok_LBrace)
  {
    if (!parseFunctionBody())
    {
      Cur = save;
      return false;
    }
    return true;
  }
  // otherwise try singleExpression
  if (parseSingleExpression())
    return true;
  Cur = save;
  return false;
}

bool Parser::parseAssignmentOperator()
{
  // assignmentOperator
  // : '*=' | '/=' | '%=' | '+=' | '-=' | '<<=' | '>>=' | '>>>=' | '&=' | '^=' | '|=' | '**=' | '??='
  switch (Cur.kind)
  {
  case TokenKind::Tok_MultiplyAssign:             // '*='
  case TokenKind::Tok_DivideAssign:               // '/='
  case TokenKind::Tok_ModulusAssign:              // '%='
  case TokenKind::Tok_PlusAssign:                 // '+='
  case TokenKind::Tok_MinusAssign:                // '-='
  case TokenKind::Tok_LeftShiftArithmeticAssign:  // '<<='
  case TokenKind::Tok_RightShiftArithmeticAssign: // '>>='
  case TokenKind::Tok_RightShiftLogicalAssign:    // '>>>='
  case TokenKind::Tok_BitAndAssign:               // '&='
  case TokenKind::Tok_BitXorAssign:               // '^='
  case TokenKind::Tok_BitOrAssign:                // '|='
  case TokenKind::Tok_PowerAssign:                // '**='
  case TokenKind::Tok_NullishCoalescingAssign:    // '??='
    advance();
    return true;
  default:
    return false;
  }
}

bool Parser::parseLiteral()
{
  // literal
  // : NullLiteral | BooleanLiteral | StringLiteral | templateStringLiteral | RegularExpressionLiteral | numericLiteral | bigintLiteral
  // simple tokens
  switch (Cur.kind)
  {
  case TokenKind::Tok_NullLiteral:
  case TokenKind::Tok_BooleanLiteral:
  case TokenKind::Tok_StringLiteral:
  case TokenKind::Tok_RegularExpressionLiteral:
    advance();
    return true;
  default:
    break;
  }

  // templateStringLiteral: BackTick templateStringAtom* BackTick
  if (parseTemplateStringLiteral())
    return true;

  // numeric and bigint literals
  if (parseNumericLiteral())
    return true;
  // bigintLiteral : BigDecimalIntegerLiteral | BigHexIntegerLiteral | BigOctalIntegerLiteral | BigBinaryIntegerLiteral
  if (parseBigintLiteral())
    return true;

  return false;
}

bool Parser::parseNumericLiteral()
{
  // numericLiteral
  // : DecimalLiteral | HexIntegerLiteral | OctalIntegerLiteral | OctalIntegerLiteral2 | BinaryIntegerLiteral
  switch (Cur.kind)
  {
  case TokenKind::Tok_DecimalLiteral:
  case TokenKind::Tok_HexIntegerLiteral:
  case TokenKind::Tok_OctalIntegerLiteral:
  case TokenKind::Tok_OctalIntegerLiteral2:
  case TokenKind::Tok_BinaryIntegerLiteral:
  case TokenKind::Tok_Integer:
    advance();
    return true;
  default:
    break;
  }
  // bigint literals handled separately at parseLiteral
  return false;
}

bool Parser::parseBigintLiteral()
{
  switch (Cur.kind)
  {
  case TokenKind::Tok_BigDecimalIntegerLiteral:
  case TokenKind::Tok_BigHexIntegerLiteral:
  case TokenKind::Tok_BigOctalIntegerLiteral:
  case TokenKind::Tok_BigBinaryIntegerLiteral:
    advance();
    return true;
  default:
    return false;
  }
}

bool Parser::parseTemplateStringLiteral()
{
  // Accept either a BackTick starting a template string or a TemplateStringAtom token emitted by lexer.
  if (Cur.kind == TokenKind::Tok_BackTick)
  {
    // consume opening backtick
    advance();
    // lexer drives template string atom/emission; accept zero or more TemplateStringAtom or TemplateStringStartExpression tokens
    while (Cur.kind == TokenKind::Tok_TemplateStringAtom || Cur.kind == TokenKind::Tok_TemplateStringStartExpression)
    {
      if (!parseTemplateStringAtom())
        return false;
    }
    // expect closing backtick token
    if (Cur.kind != TokenKind::Tok_BackTick)
      return false;
    advance();
    return true;
  }
  // If a TemplateStringAtom appears at top-level (lexer may emit atom tokens directly), accept it as a template literal fragment
  if (Cur.kind == TokenKind::Tok_TemplateStringAtom)
  {
    advance();
    return true;
  }
  return false;
}

bool Parser::parseTemplateStringAtom()
{
  // templateStringAtom
  // : TemplateStringAtom
  // | TemplateStringStartExpression singleExpression TemplateCloseBrace
  if (Cur.kind == TokenKind::Tok_TemplateStringAtom)
  {
    advance();
    return true;
  }
  if (Cur.kind == TokenKind::Tok_TemplateStringStartExpression)
  {
    // consume the start expression token
    advance();
    // parse the embedded singleExpression
    if (!parseSingleExpression())
      return false;
    // TemplateCloseBrace is emitted by the lexer when it exits template expression; expect it conservatively
    // There is no explicit TokenKind in some implementations; assume lexer will emit Tok_TemplateStringAtom or Tok_BackTick after.
    // If a dedicated token exists, handle it; otherwise accept current position.
    // For safety, do not fail if no explicit close token is present; rely on lexer state.
    return true;
  }
  return false;
}

bool Parser::parsePropertyName()
{
  // propertyName : identifierName | StringLiteral | numericLiteral | '[' singleExpression ']'
  if (Cur.kind == TokenKind::Tok_StringLiteral)
  {
    advance();
    return true;
  }
  // numericLiteral: DecimalLiteral | HexIntegerLiteral | OctalIntegerLiteral | OctalIntegerLiteral2 | BinaryIntegerLiteral
  switch (Cur.kind)
  {
  case TokenKind::Tok_DecimalLiteral:
  case TokenKind::Tok_HexIntegerLiteral:
  case TokenKind::Tok_OctalIntegerLiteral:
  case TokenKind::Tok_OctalIntegerLiteral2:
  case TokenKind::Tok_BinaryIntegerLiteral:
  case TokenKind::Tok_Integer:
    advance();
    return true;
  default:
    break;
  }
  if (Cur.kind == TokenKind::Tok_LBracket)
  {
    advance();
    if (!parseSingleExpression())
      return false;
    if (Cur.kind != TokenKind::Tok_RBracket)
      return false;
    advance();
    return true;
  }
  return parseIdentifierName();
}

bool Parser::parsePropertyAssignment()
{
  // propertyAssignment alternatives (conservative)
  auto save = Cur;
  // 1) propertyName ':' singleExpression
  if (parsePropertyName())
  {
    if (Cur.kind == TokenKind::Tok_Colon)
    {
      advance();
      if (!parseSingleExpression())
      {
        Cur = save;
        return false;
      }
      return true;
    }
    // if not colon, restore and try other forms
    Cur = save;
  }

  // 2) '[' singleExpression ']' ':' singleExpression (ComputedPropertyExpressionAssignment)
  if (Cur.kind == TokenKind::Tok_LBracket)
  {
    advance();
    if (!parseSingleExpression())
    {
      Cur = save;
      return false;
    }
    if (Cur.kind != TokenKind::Tok_RBracket)
    {
      Cur = save;
      return false;
    }
    advance();
    if (Cur.kind != TokenKind::Tok_Colon)
    {
      Cur = save;
      return false;
    }
    advance();
    if (!parseSingleExpression())
    {
      Cur = save;
      return false;
    }
    return true;
  }

  // 3) Async? '*'? propertyName '(' formalParameterList? ')' functionBody  (FunctionProperty)
  Cur = save;
  if (Cur.kind == TokenKind::Tok_Async)
    advance();
  if (Cur.kind == TokenKind::Tok_Multiply)
    advance();
  if (parsePropertyName())
  {
    if (Cur.kind == TokenKind::Tok_LParen)
    {
      advance();
      // optional formalParameterList
      if (Cur.kind != TokenKind::Tok_RParen)
      {
        if (!parseFormalParameterList())
        {
          Cur = save;
          return false;
        }
      }
      if (Cur.kind != TokenKind::Tok_RParen)
      {
        Cur = save;
        return false;
      }
      advance();
      if (!parseFunctionBody())
      {
        Cur = save;
        return false;
      }
      return true;
    }
  }
  Cur = save;

  // 4) getter '(' ')' functionBody
  if (parseGetter())
  {
    if (Cur.kind != TokenKind::Tok_LParen)
    {
      Cur = save;
      return false;
    }
    advance();
    if (Cur.kind != TokenKind::Tok_RParen)
    {
      Cur = save;
      return false;
    }
    advance();
    if (!parseFunctionBody())
    {
      Cur = save;
      return false;
    }
    return true;
  }
  Cur = save;

  // 5) setter '(' formalParameterArg ')' functionBody
  if (parseSetter())
  {
    if (Cur.kind != TokenKind::Tok_LParen)
    {
      Cur = save;
      return false;
    }
    advance();
    if (!parseFormalParameterArg())
    {
      Cur = save;
      return false;
    }
    if (Cur.kind != TokenKind::Tok_RParen)
    {
      Cur = save;
      return false;
    }
    advance();
    if (!parseFunctionBody())
    {
      Cur = save;
      return false;
    }
    return true;
  }
  Cur = save;

  // 6) Ellipsis? singleExpression  (PropertyShorthand)
  if (Cur.kind == TokenKind::Tok_Ellipsis)
  {
    advance();
    if (!parseSingleExpression())
    {
      Cur = save;
      return false;
    }
    return true;
  }
  if (parseSingleExpression())
    return true;

  return false;
}

bool Parser::parseGetter()
{
  // getter : {this.n("get")}? identifier classElementName
  // Enforce the grammar predicate {this.n("get")}?: only accept when current
  // token text equals "get" and there is no line terminator between the
  // previous token and this one. Otherwise fall back to conservative behavior.
  auto save = Cur;
  if (n("get"))
  {
    // consume the 'get' contextual keyword
    advance();
    if (!parseIdentifierName())
    {
      Cur = save;
      return false;
    }
    if (!parseClassElementName())
    {
      Cur = save;
      return false;
    }
    return true;
  }
  // Fallback: accept identifier-like name followed by classElementName
  Cur = save;
  if (!parseIdentifierName())
  {
    Cur = save;
    return false;
  }
  if (!parseClassElementName())
  {
    Cur = save;
    return false;
  }
  return true;
}

bool Parser::parseSetter()
{
  // setter : {this.n("set")}? identifier classElementName
  auto save = Cur;
  if (n("set"))
  {
    advance();
    if (!parseIdentifierName())
    {
      Cur = save;
      return false;
    }
    if (!parseClassElementName())
    {
      Cur = save;
      return false;
    }
    return true;
  }
  Cur = save;
  if (!parseIdentifierName())
  {
    Cur = save;
    return false;
  }
  if (!parseClassElementName())
  {
    Cur = save;
    return false;
  }
  return true;
}

bool Parser::n(const std::string &s)
{
  // Check current token text matches s and there is no line terminator between
  // the previous token end and the current token's start. Use the lexer's
  // ContainsLineTerminatorBetween for the check. If PrevTokenEnd is 0, allow
  // match (start-of-file).
  if (Cur.text != s)
    return false;
  if (PrevTokenEnd == 0)
    return true;
  size_t from = PrevTokenEnd;
  size_t to = Cur.pos;
  if (from >= to)
    return true; // adjacent or overlapping - treat as no LT
  return !L.ContainsLineTerminatorBetween(from, to);
}

bool Parser::parseSingleExpression()
{
  // Structured, conservative recognizer for singleExpression per grammar.
  // Try anonymousFunction first (grammar alternative order)
  {
    auto save = Cur;
    if (parseAnonymousFunction())
      return true;
    Cur = save;
  }
  // This recognizes primary expressions, `new` forms, parenthesized expressionSequence,
  // and common postfix forms (call, member, index, optional chaining) conservatively.

  // Helper: parse a primary expression
  auto parsePrimary = [&]() -> bool
  {
    switch (Cur.kind)
    {
    case TokenKind::Tok_Integer:
    case TokenKind::Tok_DecimalLiteral:
    case TokenKind::Tok_HexIntegerLiteral:
    case TokenKind::Tok_OctalIntegerLiteral:
    case TokenKind::Tok_OctalIntegerLiteral2:
    case TokenKind::Tok_BinaryIntegerLiteral:
    case TokenKind::Tok_StringLiteral:
    case TokenKind::Tok_NullLiteral:
    case TokenKind::Tok_BooleanLiteral:
    case TokenKind::Tok_This:
    case TokenKind::Tok_Super:
      advance();
      return true;
    case TokenKind::Tok_LBracket:
      return parseArrayLiteral();
    case TokenKind::Tok_LBrace:
    {
      // conservative object literal: consume until matching '}'
      int depth = 0;
      while (Cur.kind != TokenKind::Tok_EOF)
      {
        if (Cur.kind == TokenKind::Tok_LBrace)
          depth++;
        else if (Cur.kind == TokenKind::Tok_RBrace)
        {
          depth--;
          advance();
          if (depth == 0)
            return true;
          continue;
        }
        advance();
      }
      return false;
    }
    case TokenKind::Tok_LParen:
    {
      // parenthesized expressionSequence
      advance();
      if (Cur.kind == TokenKind::Tok_RParen)
      {
        advance();
        return true;
      }
      if (!parseExpressionSequence())
        return false;
      if (Cur.kind != TokenKind::Tok_RParen)
        return false;
      advance();
      return true;
    }
    case TokenKind::Tok_Function:
    case TokenKind::Tok_Class:
      // accept token conservatively
      advance();
      return true;
    default:
      // identifier-like or reserved words allowed as primary
      if (parseIdentifierName())
        return true;
      return false;
    }
  };

  // Handle prefix unary operators
  if (Cur.kind == TokenKind::Tok_PlusPlus || Cur.kind == TokenKind::Tok_MinusMinus)
  {
    advance();
    return parseSingleExpression();
  }
  if (Cur.kind == TokenKind::Tok_Plus || Cur.kind == TokenKind::Tok_Minus || Cur.kind == TokenKind::Tok_BitNot || Cur.kind == TokenKind::Tok_Not)
  {
    advance();
    return parseSingleExpression();
  }
  if (Cur.kind == TokenKind::Tok_Delete || Cur.kind == TokenKind::Tok_Void || Cur.kind == TokenKind::Tok_Typeof || Cur.kind == TokenKind::Tok_Await)
  {
    advance();
    return parseSingleExpression();
  }

  // New-expression forms
  if (Cur.kind == TokenKind::Tok_New)
  {
    advance();
    // New identifier arguments | New singleExpression arguments | New singleExpression
    if (parseIdentifierName())
    {
      // optional arguments
      if (Cur.kind == TokenKind::Tok_LParen)
      {
        if (!parseArguments())
          return false;
      }
      return true;
    }
    if (!parseSingleExpression())
      return false;
    if (Cur.kind == TokenKind::Tok_LParen)
    {
      if (!parseArguments())
        return false;
    }
    return true;
  }

  // Parse primary expression
  if (!parsePrimary())
    return false;

  // Handle postfix and member operations conservatively: call, index, member access, optional chaining, postfix ++/--
  while (true)
  {
    if (Cur.kind == TokenKind::Tok_Dot || Cur.kind == TokenKind::Tok_QuestionDot)
    {
      // consume '.' or '?.'
      advance();
      // optional private/mangled forms: '#' handled elsewhere, accept identifierName
      if (!parseIdentifierName())
        return false;
      continue;
    }
    if (Cur.kind == TokenKind::Tok_LBracket)
    {
      // index expression
      advance();
      if (!parseExpressionSequence())
        return false;
      if (Cur.kind != TokenKind::Tok_RBracket)
        return false;
      advance();
      continue;
    }
    if (Cur.kind == TokenKind::Tok_LParen)
    {
      // call arguments
      if (!parseArguments())
        return false;
      continue;
    }
    if (Cur.kind == TokenKind::Tok_PlusPlus || Cur.kind == TokenKind::Tok_MinusMinus)
    {
      // postfix inc/dec (grammar checks line terminator; we are conservative)
      advance();
      continue;
    }
    // template string continuation or other postfix tokens can be conservatively skipped
    break;
  }

  return true;
}

bool Parser::parseArguments()
{
  // arguments : '(' (argument (',' argument)* ','?)? ')'
  if (Cur.kind != TokenKind::Tok_LParen)
    return false;
  advance();
  // optional sequence of arguments
  if (Cur.kind != TokenKind::Tok_RParen)
  {
    // at least one argument
    if (!parseArgument())
      return false;
    // zero or more ',' argument
    while (Cur.kind == TokenKind::Tok_Comma)
    {
      advance();
      // allow trailing comma: if next is ')' then break
      if (Cur.kind == TokenKind::Tok_RParen)
        break;
      if (!parseArgument())
        return false;
    }
  }
  if (Cur.kind != TokenKind::Tok_RParen)
    return false;
  advance();
  return true;
}

bool Parser::parseArgument()
{
  // argument : Ellipsis? (singleExpression | identifier)
  if (Cur.kind == TokenKind::Tok_Ellipsis)
  {
    advance();
    if (!parseSingleExpression())
      return false;
    return true;
  }
  // either a singleExpression or an identifier-like token
  if (parseSingleExpression())
    return true;
  return false;
}

bool Parser::parseInitializer()
{
  // initializer : '=' singleExpression
  if (Cur.kind != TokenKind::Tok_Assign)
    return false;
  advance();
  if (!parseSingleExpression())
    return false;
  return true;
}

bool Parser::parseEos()
{
  // eos : SemiColon | EOF | {this.lineTerminatorAhead()}? | {this.closeBrace()}?
  // Accept semicolon explicitly. For EOF accept as well. For predicates
  // that rely on lexer knowledge (line terminator ahead or close brace),
  // approximate using lexer helpers. We consider '}' as an acceptable eos
  // when encountered at current token.
  if (Cur.kind == TokenKind::Tok_Semi)
  {
    advance();
    return true;
  }
  if (Cur.kind == TokenKind::Tok_EOF)
    return true;
  // If current token is a right brace, treat it as eos per grammar predicates
  if (Cur.kind == TokenKind::Tok_RBrace)
    return true;
  // lineTerminatorAhead predicate: conservatively check for a line terminator
  // between previous token end and current token start using lexer helper.
  if (PrevTokenEnd != 0)
  {
    size_t from = PrevTokenEnd;
    size_t to = Cur.pos;
    if (L.ContainsLineTerminatorBetween(from, to))
      return true;
  }
  return false;
}
