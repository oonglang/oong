#include "parser.h"

ParseResult Parser::parse() {
  // Handle optional leading hash-bang; may return an empty-Program result
  if (auto res = handleOptionalHashBang())
    return std::move(*res);

  // Parse optional sourceElements (sourceElement*) per grammar. For now we
  // accept zero or more top-level statements and return the last one when
  // present. This mirrors `program : HashBangLine? sourceElements? EOF`.
  auto parseSourceElements = [&]() -> std::optional<ParseResult> {
    // sourceElements : sourceElement*
    std::optional<ParseResult> last;
    while (true) {
      // sourceElement -> statement
      auto s = parseStatement();
      if (!s) break;
      last = std::move(*s);
      // stop at EOF or when parser indicates no more top-level statements
      if (Cur.kind == TokenKind::Tok_EOF) break;
    }
    return last;
  };

  if (auto se = parseSourceElements()) {
    // Ensure we've consumed to EOF
    if (Cur.kind != TokenKind::Tok_EOF) return error("expected EOF after source elements");
    return std::move(*se);
  }

  // empty program
  if (Cur.kind != TokenKind::Tok_EOF) return ParseResult{true, std::string(), nullptr};
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseImportDefault()
{
  // importDefault : aliasName ','
  auto save = Cur;
  if (!parseAliasName()) return false;
  if (Cur.kind != TokenKind::Tok_Comma) {
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
  // importFromBlock
  //   : importDefault? (importNamespace | importModuleItems) importFrom eos
  //   | StringLiteral eos

  if (Cur.kind == TokenKind::Tok_StringLiteral) {
    advance();
    if (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF) return false;
    return true;
  }

  // optional importDefault
  auto saved = Cur;
  if (parseImportDefault()) {
    // consumed default import + comma
  } else {
    // ensure we didn't consume tokens
    Cur = saved;
  }

  // must have importNamespace or importModuleItems (or identifierName as shorthand)
  // Try to detect the forms by token kind
  if (Cur.kind == TokenKind::Tok_Multiply) {
    if (!parseImportNamespace()) return false;
  } else if (Cur.kind == TokenKind::Tok_LBrace) {
    if (!parseImportModuleItems()) return false;
  } else if (parseIdentifierName()) {
    // identifierName consumed an identifier-like token; try to interpret as namespace
    // if an 'as' follows it will be handled by parseImportNamespace
    // If this isn't actually a valid import form, subsequent checks will fail.
  } else {
    return false;
  }

  // expect importFrom (the 'from' keyword)
  if (!parseImportFrom()) return false;
  return true;
}

bool Parser::parseImportNamespace()
{
  // importNamespace : ('*' | identifierName) (As identifierName)?
  if (Cur.kind == TokenKind::Tok_Multiply) {
    advance();
  } else {
    if (!parseIdentifierName()) return false;
  }
  if (Cur.kind == TokenKind::Tok_As) {
    advance();
    if (!parseIdentifierName()) return false;
  }
  return true;
}

bool Parser::parseImportFrom()
{
  // importFrom : From StringLiteral
  if (Cur.kind != TokenKind::Tok_From) return false;
  advance();
  if (Cur.kind != TokenKind::Tok_StringLiteral) return false;
  advance();
  // accept semicolon or EOF as eos
  if (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF) return false;
  return true;
}

std::optional<ParseResult> Parser::parseImportStatement()
{
  // importStatement : Import importFromBlock
  if (Cur.kind != TokenKind::Tok_Import) return std::nullopt;
  advance();
  if (!parseImportFromBlock()) return error("invalid import statement");
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseAliasName()
{
  // aliasName : identifierName (As identifierName)?
  if (!parseIdentifierName()) return false;
  if (Cur.kind == TokenKind::Tok_As) {
    advance();
    if (!parseIdentifierName()) return false;
  }
  return true;
}

std::optional<ParseResult> Parser::parseExportStatement()
{
  // exportStatement
  if (Cur.kind != TokenKind::Tok_Export) return std::nullopt;
  advance();
  // optional Default
  if (Cur.kind == TokenKind::Tok_Default) {
    advance();
    // Export Default singleExpression eos
    // For now, treat singleExpression as a single token/construct and accept any non-eos token sequence conservatively.
    // We'll be conservative: if we see an eos token, return success, otherwise consume one token and expect eos.
    if (Cur.kind == TokenKind::Tok_Semi || Cur.kind == TokenKind::Tok_EOF) return ParseResult{true, std::string(), nullptr};
    // consume a token as part of the expression
    advance();
    if (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF) return error("expected end of export default");
    return ParseResult{true, std::string(), nullptr};
  }

  // Export Default? (exportFromBlock | declaration) eos
  if (parseExportFromBlock()) return ParseResult{true, std::string(), nullptr};
  // declaration (variable/class/function)
  if (auto decl = parseDeclaration()) {
    // expect eos
    if (Cur.kind == TokenKind::Tok_Semi) advance();
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
  if (parseImportNamespace()) {
    if (!parseImportFrom()) { Cur = saved; }
    else return true;
  }

  // exportModuleItems importFrom? eos
  if (parseExportModuleItems()) {
    // optional importFrom
    if (parseImportFrom()) return true;
    // accept eos
    if (Cur.kind == TokenKind::Tok_Semi || Cur.kind == TokenKind::Tok_EOF) return true;
    return false;
  }
  return false;
}

bool Parser::parseExportModuleItems()
{
  // exportModuleItems : '{' (exportAliasName ',')* (exportAliasName ','?)? '}'
  if (Cur.kind != TokenKind::Tok_LBrace) return false;
  advance();
  while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF) {
    if (!parseExportAliasName()) return false;
    if (Cur.kind == TokenKind::Tok_Comma) {
      advance();
      if (Cur.kind == TokenKind::Tok_RBrace) break;
      continue;
    }
    break;
  }
  if (Cur.kind != TokenKind::Tok_RBrace) return false;
  advance();
  return true;
}

bool Parser::parseExportAliasName()
{
  // exportAliasName : moduleExportName (As moduleExportName)?
  if (!parseModuleExportName()) return false;
  if (Cur.kind == TokenKind::Tok_As) {
    advance();
    if (!parseModuleExportName()) return false;
  }
  return true;
}

bool Parser::parseModuleExportName()
{
  // moduleExportName : identifierName | StringLiteral
  if (Cur.kind == TokenKind::Tok_StringLiteral) { advance(); return true; }
  return parseIdentifierName();
}

bool Parser::parseImportModuleItems()
{
  // importModuleItems : '{' (importAliasName ',')* (importAliasName ','?)? '}'
  if (Cur.kind != TokenKind::Tok_LBrace) return false;
  advance();
  while (Cur.kind != TokenKind::Tok_RBrace && Cur.kind != TokenKind::Tok_EOF) {
    if (!parseImportAliasName()) return false;
    if (Cur.kind == TokenKind::Tok_Comma) {
      advance();
      if (Cur.kind == TokenKind::Tok_RBrace) break;
      continue;
    }
    break;
  }
  if (Cur.kind != TokenKind::Tok_RBrace) return false;
  advance();
  return true;
}

bool Parser::parseImportAliasName()
{
  // importAliasName : moduleExportName (As importedBinding)?
  if (!parseModuleExportName()) return false;
  if (Cur.kind == TokenKind::Tok_As) {
    advance();
    if (!parseImportedBinding()) return false;
  }
  return true;
}

std::optional<ParseResult> Parser::parsePrintStatement()
{
  if (Cur.kind != TokenKind::Tok_Print)
    return std::nullopt;
  advance();
  if (Cur.kind != TokenKind::Tok_LParen) return error("expected '('");
  advance();
  if (Cur.kind != TokenKind::Tok_Integer) return error("expected integer");
  if (!Cur.intValue.has_value()) return error("invalid integer token");
  int val = static_cast<int>(*Cur.intValue);
  advance();
  if (Cur.kind != TokenKind::Tok_RParen) return error("expected ')'");
  advance();
  return ParseResult{true, std::string(), std::make_unique<PrintStmt>(val)};
}

bool Parser::parseImportedBinding()
{
  // importedBinding : Identifier | Yield | Await
  switch (Cur.kind) {
  case TokenKind::Tok_NonStrictLet:
  case TokenKind::Tok_StrictLet:
  case TokenKind::Tok_Async:
  case TokenKind::Tok_As:
  case TokenKind::Tok_From:
  case TokenKind::Tok_Yield:
  case TokenKind::Tok_Of:
  case TokenKind::Tok_Await:
    advance();
    return true;
  default:
    return false;
  }
}

bool Parser::parseIdentifierName()
{
  // identifierName : identifier | reservedWord
  // Accept token kinds that can serve as identifier in this tokenizer.
  switch (Cur.kind) {
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
  if (Cur.kind == TokenKind::Tok_RBrace) {
    advance();
    return ParseResult{true, std::string(), nullptr};
  }
  if (auto sl = parseStatementList()) {
    if (Cur.kind != TokenKind::Tok_RBrace) return error("expected '}'");
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
  while (true) {
    auto stmt = parseStatement();
    if (!stmt) break;
    last = std::move(*stmt);
    any = true;
    if (Cur.kind == TokenKind::Tok_RBrace || Cur.kind == TokenKind::Tok_EOF) break;
  }
  if (!any) return std::nullopt;
  return last;
}

std::optional<ParseResult> Parser::parseStatement()
{
  // Try block
  if (auto b = parseBlock()) return b;
  // variable statement
  if (auto vs = parseVariableStatement()) return vs;
  // empty statement
  if (auto e = parseEmptyStatement()) return e;
  // import/export/print
  if (auto imp = parseImportStatement()) return imp;
  if (auto exp = parseExportStatement()) return exp;
  if (auto p = parsePrintStatement()) return p;
  // expression statement
  // labelled statement (Identifier ':' statement) should be tried before expression statements
  if (auto ls = parseLabelledStatement()) return ls;
  // expression statement
  if (auto es = parseExpressionStatement()) return es;
  // if statement
  if (auto iff = parseIfStatement()) return iff;
  // iteration statements
  if (auto it = parseIterationStatement()) return it;
  // continue/break/return/yield
  if (auto c = parseContinueStatement()) return c;
  if (auto b = parseBreakStatement()) return b;
  if (auto r = parseReturnStatement()) return r;
  if (auto y = parseYieldStatement()) return y;
  // with statement
  if (auto w = parseWithStatement()) return w;
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
  if (Cur.kind != TokenKind::Tok_Semi) return std::nullopt;
  advance();
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseExpressionSequence()
{
  // expressionSequence : singleExpression (',' singleExpression)*
  if (!parseSingleExpression()) return false;
  while (Cur.kind == TokenKind::Tok_Comma) {
    advance();
    if (!parseSingleExpression()) return false;
  }
  return true;
}

std::optional<ParseResult> Parser::parseExpressionStatement()
{
  // Guard from grammar: {this.notOpenBraceAndNotFunction()}? expressionSequence eos
  // We'll conservatively assume the guard passes when current token is not '{' and not 'function' keyword
  if (Cur.kind == TokenKind::Tok_LBrace || Cur.kind == TokenKind::Tok_Function) return std::nullopt;
  auto save = Cur;
  if (!parseExpressionSequence()) { Cur = save; return std::nullopt; }
  // accept eos
  if (Cur.kind == TokenKind::Tok_Semi) advance();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseIfStatement()
{
  // ifStatement : If '(' expressionSequence ')' statement (Else statement)?
  if (Cur.kind != TokenKind::Tok_If) return std::nullopt;
  advance();
  if (Cur.kind != TokenKind::Tok_LParen) return error("expected '(' after if");
  advance();
  if (!parseExpressionSequence()) return error("invalid if condition");
  if (Cur.kind != TokenKind::Tok_RParen) return error("expected ')' after if condition");
  advance();
  // parse the statement after the if
  if (auto stmt = parseStatement()) {
    // optional else
    if (Cur.kind == TokenKind::Tok_Else) {
      advance();
      if (!parseStatement()) return error("expected statement after else");
    }
    return std::move(*stmt);
  }
  return error("invalid if body");
}

std::optional<ParseResult> Parser::parseIterationStatement()
{
  // iterationStatement covers Do..While, While, For, ForIn, ForOf
  auto save = Cur;
  if (Cur.kind == TokenKind::Tok_Do) {
    advance();
    // body
    if (!parseStatement()) return error("expected statement after do");
    if (Cur.kind != TokenKind::Tok_While) return error("expected 'while' after do statement");
    advance();
    if (Cur.kind != TokenKind::Tok_LParen) return error("expected '(' after while");
    advance();
    if (!parseExpressionSequence()) return error("invalid while condition");
    if (Cur.kind != TokenKind::Tok_RParen) return error("expected ')' after while condition");
    advance();
    // expect eos
    if (Cur.kind == TokenKind::Tok_Semi) advance();
    return ParseResult{true, std::string(), nullptr};
  }

  if (Cur.kind == TokenKind::Tok_While) {
    advance();
    if (Cur.kind != TokenKind::Tok_LParen) return error("expected '(' after while");
    advance();
    if (!parseExpressionSequence()) return error("invalid while condition");
    if (Cur.kind != TokenKind::Tok_RParen) return error("expected ')' after while condition");
    advance();
    if (auto stmt = parseStatement()) return std::move(*stmt);
    return error("invalid while body");
  }

  if (Cur.kind == TokenKind::Tok_For) {
    advance();
    bool isAwait = false;
    // For Await? '(' ... ')'
    if (Cur.kind == TokenKind::Tok_Await) { isAwait = true; advance(); }
    if (Cur.kind != TokenKind::Tok_LParen) return error("expected '(' after for");
    advance();
    // Try (expressionSequence | variableDeclarationList)?
    if (Cur.kind != TokenKind::Tok_Semi) {
      auto save2 = Cur;
      if (!parseExpressionSequence()) {
        Cur = save2;
        if (!parseVariableDeclarationList()) { Cur = save; return error("invalid for initializer"); }
      }
    }
    if (Cur.kind != TokenKind::Tok_Semi) return error("expected ';' in for");
    advance();
    // optional expressionSequence
    if (Cur.kind != TokenKind::Tok_Semi) {
      if (!parseExpressionSequence()) return error("invalid for condition");
    }
    if (Cur.kind != TokenKind::Tok_Semi) return error("expected second ';' in for");
    advance();
    // optional expressionSequence
    if (Cur.kind != TokenKind::Tok_RParen) {
      if (!parseExpressionSequence()) return error("invalid for increment");
    }
    if (Cur.kind != TokenKind::Tok_RParen) return error("expected ')' after for");
    advance();
    if (auto stmt = parseStatement()) return std::move(*stmt);
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
  if (Cur.kind != TokenKind::Tok_Continue) return std::nullopt;
  advance();
  // optional identifier (we allow identifier-like tokens)
  if (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF) {
    parseIdentifierName();
  }
  if (Cur.kind == TokenKind::Tok_Semi) advance();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseBreakStatement()
{
  // Break ({this.notLineTerminator()}? identifier)? eos
  if (Cur.kind != TokenKind::Tok_Break) return std::nullopt;
  // capture break token position
  Token breakTok = Cur;
  advance();
  // Only accept an optional identifier if there is no line terminator between
  // the end of the 'break' token and the start of the following token.
  if (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF) {
    size_t from = breakTok.pos + breakTok.text.size();
    size_t to = Cur.pos;
    if (!L.ContainsLineTerminatorBetween(from, to)) {
      parseIdentifierName();
    }
  }
  if (Cur.kind == TokenKind::Tok_Semi) advance();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseReturnStatement()
{
  // Return ({this.notLineTerminator()}? expressionSequence)? eos
  if (Cur.kind != TokenKind::Tok_Return) return std::nullopt;
  // capture return token position
  Token returnTok = Cur;
  advance();
  // Only attempt to parse an expressionSequence if there is no line terminator between
  // the end of the 'return' token and the start of the next token.
  if (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF) {
    size_t from = returnTok.pos + returnTok.text.size();
    size_t to = Cur.pos;
    if (!L.ContainsLineTerminatorBetween(from, to)) {
      // attempt to parse expressionSequence (conservative; ignore failure)
      parseExpressionSequence();
    }
  }
  if (Cur.kind == TokenKind::Tok_Semi) advance();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseYieldStatement()
{
  // (Yield | YieldStar) ({this.notLineTerminator()}? expressionSequence)? eos
  if (Cur.kind != TokenKind::Tok_Yield && Cur.kind != TokenKind::Tok_YieldStar) return std::nullopt;
  // capture yield token
  Token yieldTok = Cur;
  advance();
  // Only parse the optional expressionSequence if there's no line terminator between
  // the yield token and the following token.
  if (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF) {
    size_t from = yieldTok.pos + yieldTok.text.size();
    size_t to = Cur.pos;
    if (!L.ContainsLineTerminatorBetween(from, to)) {
      parseExpressionSequence();
    }
  }
  if (Cur.kind == TokenKind::Tok_Semi) advance();
  return ParseResult{true, std::string(), nullptr};
}

std::optional<ParseResult> Parser::parseWithStatement()
{
  // withStatement : With '(' expressionSequence ')' statement
  if (Cur.kind != TokenKind::Tok_With) return std::nullopt;
  advance();
  if (Cur.kind != TokenKind::Tok_LParen) return error("expected '(' after with");
  advance();
  if (!parseExpressionSequence()) return error("invalid with expression");
  if (Cur.kind != TokenKind::Tok_RParen) return error("expected ')' after with expression");
  advance();
  if (auto stmt = parseStatement()) return std::move(*stmt);
  return error("invalid with body");
}

std::optional<ParseResult> Parser::parseLabelledStatement()
{
  // labelledStatement
  //   : Identifier ':' statement
  // Conservative implementation using existing helpers: try to consume an identifier-like token
  auto saved = Cur;
  if (!parseIdentifierName()) return std::nullopt;
  if (Cur.kind != TokenKind::Tok_Colon) {
    // restore conservative state (note: lexer state isn't restored; follows repo pattern)
    Cur = saved;
    return std::nullopt;
  }
  // consume ':'
  advance();
  auto inner = parseStatement();
  if (!inner) return error("invalid labelled statement body");
  return std::move(*inner);
}

std::optional<ParseResult> Parser::parseDeclaration()
{
  // declaration : variableStatement | classDeclaration | functionDeclaration
  // For now we implement a conservative recognition:
  // - variableStatement: starts with Var/let_/Const
  // - classDeclaration: starts with Class
  // - functionDeclaration: starts with Function_
  switch (Cur.kind) {
  case TokenKind::Tok_Var:
  case TokenKind::Tok_NonStrictLet:
  case TokenKind::Tok_StrictLet:
  case TokenKind::Tok_Const: {
    // variableStatement -> variableDeclarationList eos
    // Consume the var/let/const token and then accept until eos conservatively.
    advance();
    // consume tokens until we hit semicolon/EOF or parser can't progress.
    while (Cur.kind != TokenKind::Tok_Semi && Cur.kind != TokenKind::Tok_EOF) {
      // rudimentary consumption: stop on tokens that likely start a new statement/block
      if (Cur.kind == TokenKind::Tok_LBrace || Cur.kind == TokenKind::Tok_RBrace) break;
      advance();
    }
    if (Cur.kind == TokenKind::Tok_Semi) advance();
    return ParseResult{true, std::string(), nullptr};
  }
  case TokenKind::Tok_Class: {
    // classDeclaration : Class identifier classTail
    advance();
    // optional identifier
    if (parseIdentifierName()) {
      // continue
    }
    // classTail starts with optional Extends or '{'
    // Consume until matching '}' or EOF conservatively
    if (Cur.kind == TokenKind::Tok_Extends) {
      advance();
      // consume a single expression token (conservative)
      if (Cur.kind != TokenKind::Tok_EOF) advance();
    }
    if (Cur.kind == TokenKind::Tok_LBrace) {
      // consume until matching '}'
      int depth = 0;
      while (Cur.kind != TokenKind::Tok_EOF) {
        if (Cur.kind == TokenKind::Tok_LBrace) depth++;
        else if (Cur.kind == TokenKind::Tok_RBrace) {
          depth--;
          if (depth == 0) { advance(); break; }
        }
        advance();
      }
    }
    return ParseResult{true, std::string(), nullptr};
  }
  case TokenKind::Tok_Function: {
    // functionDeclaration : Async? Function_ '*'? identifier '(' formalParameterList? ')' functionBody
    // We conservatively accept a function by consuming tokens until the function body ends.
    if (Cur.kind == TokenKind::Tok_Async) advance();
    if (Cur.kind != TokenKind::Tok_Function) return std::nullopt;
    advance();
    // optional '*' (not tokenized specially here)
    // optional identifier
    if (parseIdentifierName()) {
      // ok
    }
    // skip to next '{'
    while (Cur.kind != TokenKind::Tok_LBrace && Cur.kind != TokenKind::Tok_EOF) advance();
    if (Cur.kind == TokenKind::Tok_LBrace) {
      // consume functionBody
      int depth = 0;
      while (Cur.kind != TokenKind::Tok_EOF) {
        if (Cur.kind == TokenKind::Tok_LBrace) depth++;
        else if (Cur.kind == TokenKind::Tok_RBrace) {
          depth--;
          if (depth == 0) { advance(); break; }
        }
        advance();
      }
    }
    return ParseResult{true, std::string(), nullptr};
  }
  default:
    return std::nullopt;
  }
}

std::optional<ParseResult> Parser::parseVariableStatement()
{
  // variableStatement : variableDeclarationList eos
  auto save = Cur;
  if (!parseVariableDeclarationList()) { Cur = save; return std::nullopt; }
  // accept semicolon or EOF as eos
  if (Cur.kind == TokenKind::Tok_Semi) advance();
  return ParseResult{true, std::string(), nullptr};
}

bool Parser::parseVariableDeclarationList()
{
  // variableDeclarationList : varModifier variableDeclaration (',' variableDeclaration)*
  if (!parseVarModifier()) return false;
  if (!parseVariableDeclaration()) return false;
  while (Cur.kind == TokenKind::Tok_Comma) {
    advance();
    if (!parseVariableDeclaration()) return false;
  }
  return true;
}

bool Parser::parseVarModifier()
{
  // varModifier : Var | let_ | Const
  switch (Cur.kind) {
  case TokenKind::Tok_Var:
  case TokenKind::Tok_NonStrictLet:
  case TokenKind::Tok_StrictLet:
  case TokenKind::Tok_Const:
    advance();
    return true;
  default:
    return false;
  }
}

bool Parser::parseVariableDeclaration()
{
  // variableDeclaration : assignable ('=' singleExpression)?
  if (!parseAssignable()) return false;
  if (Cur.kind == TokenKind::Tok_Assign) {
    advance();
    if (!parseSingleExpression()) return false;
  }
  return true;
}

bool Parser::parseAssignable()
{
  // assignable : identifier | keyword | arrayLiteral | objectLiteral
  // Accept identifier-like tokens
  if (parseIdentifierName()) return true;
  // object or array literal starts
  if (Cur.kind == TokenKind::Tok_LBrace) {
    // consume a conservative object literal until matching '}'
    int depth = 0;
    while (Cur.kind != TokenKind::Tok_EOF) {
      if (Cur.kind == TokenKind::Tok_LBrace) depth++;
      else if (Cur.kind == TokenKind::Tok_RBrace) {
        depth--;
        advance();
        if (depth == 0) return true;
        continue;
      }
      advance();
    }
    return false;
  }
  if (Cur.kind == TokenKind::Tok_LBracket) {
    // consume array literal until matching ']'
    int depth = 0;
    while (Cur.kind != TokenKind::Tok_EOF) {
      if (Cur.kind == TokenKind::Tok_LBracket) depth++;
      else if (Cur.kind == TokenKind::Tok_RBracket) {
        depth--;
        advance();
        if (depth == 0) return true;
        continue;
      }
      advance();
    }
    return false;
  }
  return false;
}

bool Parser::parseSingleExpression()
{
  // Very conservative singleExpression recognizer for initializers
  switch (Cur.kind) {
  case TokenKind::Tok_Integer:
  case TokenKind::Tok_DecimalLiteral:
  case TokenKind::Tok_StringLiteral:
  case TokenKind::Tok_NullLiteral:
  case TokenKind::Tok_BooleanLiteral:
  case TokenKind::Tok_This:
  case TokenKind::Tok_Super:
    advance();
    return true;
  case TokenKind::Tok_LParen: {
    // consume until matching ')'
    int depth = 0;
    while (Cur.kind != TokenKind::Tok_EOF) {
      if (Cur.kind == TokenKind::Tok_LParen) depth++;
      else if (Cur.kind == TokenKind::Tok_RParen) {
        depth--;
        advance();
        if (depth == 0) return true;
        continue;
      }
      advance();
    }
    return false;
  }
  case TokenKind::Tok_New:
  case TokenKind::Tok_Function:
  case TokenKind::Tok_Class:
    // accept the token and stop (conservative)
    advance();
    return true;
  default:
    // accept identifier-like and other single tokens
    if (parseIdentifierName()) return true;
    return false;
  }
}
