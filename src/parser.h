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
  bool parseIdentifier();
  bool parseReservedWord();
  bool parseKeyword();
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
  std::optional<ParseResult> parseFunctionDeclaration();
  std::optional<ParseResult> parseClassDeclaration();
  bool parseClassTail();
  bool parseClassElement();
  bool parseMethodDefinition();
  bool parseFieldDefinition();
  bool parseClassElementName();
  bool parseInitializer();
  bool parseObjectLiteral();
  bool parsePrivateIdentifier();
  bool parseAnonymousFunction();
  bool parseArrowFunctionParameters();
  bool parseArrowFunctionBody();
  bool parseAssignmentOperator();
  bool parseLiteral();
  bool parseTemplateStringLiteral();
  bool parseTemplateStringAtom();
  bool parseNumericLiteral();
  bool parseBigintLiteral();
  bool parseFormalParameterList();
  bool parseFormalParameterArg();
  bool parseLastFormalParameterArg();
  bool parseArrayLiteral();
  bool parseElementList();
  bool parseArrayElement();
  bool parsePropertyAssignment();
  bool parsePropertyName();
  bool parseGetter();
  bool parseSetter();
  bool parseArguments();
  bool parseArgument();
  // Variable statement parsing: variableStatement, variableDeclarationList, variableDeclaration
  std::optional<ParseResult> parseVariableStatement();
  bool parseVariableDeclarationList();
  bool parseVariableDeclaration();
  bool parseAssignable();
  bool parseSingleExpression();
  bool parseVarModifier();
  bool parseLet_();
  bool parseEos();
  bool parseExpressionSequence();
  std::optional<ParseResult> parseExpressionStatement();
  std::optional<ParseResult> parseIfStatement();
  std::optional<ParseResult> parseIterationStatement();
  std::optional<ParseResult> parseContinueStatement();
  std::optional<ParseResult> parseBreakStatement();
  std::optional<ParseResult> parseReturnStatement();
  std::optional<ParseResult> parseYieldStatement();
  std::optional<ParseResult> parseWithStatement();
  std::optional<ParseResult> parseThrowStatement();
  std::optional<ParseResult> parseTryStatement();
  bool parseCatchProduction();
  bool parseFinallyProduction();
  std::optional<ParseResult> parseDebuggerStatement();
  std::optional<ParseResult> parseSwitchStatement();
  bool parseCaseBlock();
  bool parseCaseClauses();
  bool parseCaseClause();
  bool parseDefaultClause();
  std::optional<ParseResult> parseLabelledStatement();
  std::optional<ParseResult> parseEmptyStatement();
  // Parse a block: '{' statementList? '}'
  std::optional<ParseResult> parseBlock();
  bool parseFunctionBody();
  std::optional<ParseResult> parseSourceElements();
  // Parse one-or-more statements
  std::optional<ParseResult> parseStatementList();
  Lexer L;
  Token Cur;
  // position (byte index) immediately after the previous token in the source.
  // Used to implement grammar predicates that need to check for line terminators
  // between tokens (e.g. this.n("get"), lineTerminatorAhead, etc.).
  size_t PrevTokenEnd{0};
  // Grammar predicate helper: return true when the current token's text equals
  // `s` and there is no line terminator between the previous token and the
  // current token (approximates ANTLR's this.n("...") predicate).
  bool n(const std::string &s);
  void advance() { PrevTokenEnd = Cur.pos + Cur.text.size(); Cur = L.nextToken(); }
  ParseResult error(const std::string &msg) { return {false, msg, nullptr}; }
};
