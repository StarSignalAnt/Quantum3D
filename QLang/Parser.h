#pragma once

#include "QAssign.h"
#include "QClass.h"
#include "QEnum.h"
#include "QError.h"
#include "QExpression.h"
#include "QFor.h"
#include "QIf.h"
#include "QIncrement.h"
#include "QInstanceDecl.h"
#include "QMemberAssign.h"
#include "QMethod.h"
#include "QMethodCall.h"
#include "QParameters.h"
#include "QProgram.h"
#include "QReturn.h"
#include "QStatement.h"
#include "QVariableDecl.h"
#include "QWhile.h"
#include "Tokenizer.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

// Parser class
class Parser {
public:
  Parser(const std::vector<Token> &tokens);
  Parser(const std::vector<Token> &tokens,
         std::shared_ptr<QErrorCollector> errorCollector);
  ~Parser();

  std::shared_ptr<QProgram> Parse();

  // Error access
  std::shared_ptr<QErrorCollector> GetErrorCollector() const {
    return m_ErrorCollector;
  }
  bool HasErrors() const {
    return m_ErrorCollector && m_ErrorCollector->HasErrors();
  }

  // Register external class names (e.g., from engine classes)
  void RegisterKnownClasses(const std::set<std::string> &classNames) {
    m_ClassNames.insert(classNames.begin(), classNames.end());
  }
  void RegisterKnownClass(const std::string &className) {
    m_ClassNames.insert(className);
  }

private:
  std::vector<Token> m_Tokens;
  int m_Current = 0;
  std::set<std::string> m_ClassNames; // Track known class names
  std::set<std::string> m_EnumNames;  // Track known enum names
  std::set<std::string>
      m_DeclaredVariables; // Track declared variables in current scope
  std::set<std::string>
      m_ClassMemberVariables; // Track class member variables (fields)
  std::set<std::string>
      m_ForwardDeclaredClasses; // Track forward-declared class names
  std::vector<std::string>
      m_CurrentTypeParams; // Track current generic parameters (T, K, V)
  std::shared_ptr<QErrorCollector> m_ErrorCollector;

  // Context tracking for error reporting
  std::string m_CurrentContext;
  int m_CurrentContextStartLine = 0;

  // Error reporting helper
  void ReportError(const std::string &message,
                   QErrorSeverity severity = QErrorSeverity::Error);
  void RecoverToNextStatement(); // Skip to next statement for recovery

  // Parsing methods
  std::shared_ptr<QProgram> ParseProgram();
  void ParseCode(std::shared_ptr<QCode> code);
  std::shared_ptr<QStatement> ParseStatement();
  std::shared_ptr<QVariableDecl> ParseVariableDecl();
  std::shared_ptr<QClass> ParseClass();
  std::shared_ptr<QEnum> ParseEnum();
  std::shared_ptr<QMethod> ParseMethod();
  std::shared_ptr<QInstanceDecl> ParseInstanceDecl();
  std::shared_ptr<QMethodCall> ParseMethodCall();
  std::shared_ptr<QMemberAssign> ParseMemberAssign();
  std::shared_ptr<QAssign> ParseAssign();
  std::shared_ptr<QVariableDecl> ParseClassTypeMember();
  std::shared_ptr<QReturn> ParseReturn();
  std::shared_ptr<QIf> ParseIf();
  std::shared_ptr<QFor> ParseFor();
  std::shared_ptr<QWhile> ParseWhile();
  std::shared_ptr<QIncrement> ParseIncrement();
  std::shared_ptr<QParameters> ParseParameters();
  std::shared_ptr<QExpression> ParseExpression();

  // Helper methods
  Token Peek() const;
  Token PeekNext() const; // Look ahead one more token
  Token Previous() const;
  Token Advance();
  bool IsAtEnd() const;
  bool Check(TokenType type) const;
  Token Consume(TokenType type, const std::string &message);
  bool Match(TokenType type);
  bool IsTypeToken(TokenType type) const;
  bool IsClassName(const std::string &name) const;
};
