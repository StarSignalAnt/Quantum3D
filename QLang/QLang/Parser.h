#pragma once

#include "QClass.h"
#include "QExpression.h"
#include "QInstanceDecl.h"
#include "QMemberAssign.h"
#include "QMethod.h"
#include "QMethodCall.h"
#include "QParameters.h"
#include "QProgram.h"
#include "QReturn.h"
#include "QStatement.h"
#include "QVariableDecl.h"
#include "Tokenizer.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

// Parser class
class Parser {
public:
  Parser(const std::vector<Token> &tokens);
  ~Parser();

  std::shared_ptr<QProgram> Parse();

private:
  std::vector<Token> m_Tokens;
  int m_Current = 0;
  std::set<std::string> m_ClassNames; // Track known class names

  // Parsing methods
  std::shared_ptr<QProgram> ParseProgram();
  void ParseCode(std::shared_ptr<QCode> code);
  std::shared_ptr<QStatement> ParseStatement();
  std::shared_ptr<QVariableDecl> ParseVariableDecl();
  std::shared_ptr<QClass> ParseClass();
  std::shared_ptr<QMethod> ParseMethod();
  std::shared_ptr<QInstanceDecl> ParseInstanceDecl();
  std::shared_ptr<QMethodCall> ParseMethodCall();
  std::shared_ptr<QMemberAssign> ParseMemberAssign();
  std::shared_ptr<QVariableDecl> ParseClassTypeMember();
  std::shared_ptr<QReturn> ParseReturn();
  std::shared_ptr<QParameters> ParseParameters();
  std::shared_ptr<QExpression> ParseExpression();

  // Helper methods
  Token Peek() const;
  Token PeekNext() const; // Look ahead one more token
  Token Previous() const;
  Token Advance();
  bool IsAtEnd() const;
  bool Check(TokenType type) const;
  bool Match(TokenType type);
  bool IsTypeToken(TokenType type) const;
  bool IsClassName(const std::string &name) const;
};
