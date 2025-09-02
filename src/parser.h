#pragma once
#include "lexer.h"
#include <memory>
#include <optional>
#include <string>

#include "ast.h"

// Parser result
struct ParseResult {
  bool ok;
  std::string error;
  std::unique_ptr<Stmt> stmt;
};

class Parser {
public:
  explicit Parser(const std::string &src) : L(src) { Cur = L.nextToken(); }
  ParseResult parse();

private:
  // Handle optional leading HashBangLine per grammar: consume a leading Tok_Hashtag
  // and return an optional ParseResult when the file contains only a hash-bang line.
  std::optional<ParseResult> handleOptionalHashBang();
  // Try to parse a top-level print statement: 'print' '(' INTEGER ')' EOF
  // Returns an optional ParseResult: present if this rule matched (success or error),
  // or std::nullopt to indicate the rule does not apply.
  std::optional<ParseResult> parsePrintStatement();
  // Try to parse an import statement per grammar: Import importFromBlock
  // Supports `import "module";` and a simple namespace import form `import * from "module";`.
  std::optional<ParseResult> parseImportStatement();
  // Helpers for parsing `import { ... } from "module"` forms
  bool parseImportModuleItems();
  bool parseImportAliasName();
  bool parseModuleExportName();
  bool parseImportedBinding();
  bool parseAliasName();
  bool parseImportNamespace();
  bool parseIdentifierName();
  bool parseImportDefault();
  bool parseImportFromBlock();
  bool parseImportFrom();
  // Export parsing helpers
  std::optional<ParseResult> parseExportStatement();
  bool parseExportFromBlock();
  bool parseExportModuleItems();
  bool parseExportAliasName();
  // Try to parse any top-level statement. Returns a ParseResult when matched or
  // std::nullopt to indicate the rule does not apply.
  std::optional<ParseResult> parseStatement();
  // Parse a declaration: variableStatement | classDeclaration | functionDeclaration
  std::optional<ParseResult> parseDeclaration();
  // Variable statement parsing: variableStatement, variableDeclarationList, variableDeclaration
  std::optional<ParseResult> parseVariableStatement();
  bool parseVariableDeclarationList();
  bool parseVariableDeclaration();
  bool parseAssignable();
  bool parseSingleExpression();
  bool parseVarModifier();
  bool parseExpressionSequence();
  std::optional<ParseResult> parseExpressionStatement();
  std::optional<ParseResult> parseIfStatement();
  std::optional<ParseResult> parseIterationStatement();
  std::optional<ParseResult> parseContinueStatement();
  std::optional<ParseResult> parseBreakStatement();
  std::optional<ParseResult> parseReturnStatement();
  std::optional<ParseResult> parseYieldStatement();
  std::optional<ParseResult> parseWithStatement();
  std::optional<ParseResult> parseLabelledStatement();
  std::optional<ParseResult> parseEmptyStatement();
  // Parse a block: '{' statementList? '}'
  std::optional<ParseResult> parseBlock();
  // Parse one-or-more statements
  std::optional<ParseResult> parseStatementList();
  Lexer L;
  Token Cur;
  void advance() { Cur = L.nextToken(); }
  ParseResult error(const std::string &msg) { return {false, msg, nullptr}; }
};
