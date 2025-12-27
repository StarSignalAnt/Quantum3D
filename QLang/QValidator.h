#pragma once

#include "QError.h"
#include "QProgram.h"
#include <memory>
#include <set>
#include <string>

// Forward declarations
class QClass;
class QMethod;
class QCode;
class QNode;
class QExpression;
class QVariableDecl;
class QAssign;
class QMemberAssign;
class QMethodCall;
class QStatement;
class QIf;
class QFor;
class QWhile;
class QReturn;

// QValidator - Performs semantic analysis on parsed QProgram AST
class QValidator {
public:
  QValidator(std::shared_ptr<QErrorCollector> errorCollector);

  // Main validation entry point
  bool Validate(std::shared_ptr<QProgram> program);

  // Register known classes from engine or previously compiled modules
  void RegisterKnownClass(const std::string &className);
  void RegisterKnownClasses(const std::set<std::string> &classNames);

private:
  std::shared_ptr<QErrorCollector> m_ErrorCollector;
  std::set<std::string> m_KnownClasses;           // Classes from engine/modules
  std::set<std::string> m_DeclaredClasses;        // Classes in current program
  std::set<std::string> m_ForwardDeclaredClasses; // Forward-declared classes
  std::set<std::string> m_DeclaredEnums;          // Enums in current program
  std::set<std::string> m_CurrentScopeVars;       // Variables in current scope
  std::set<std::string> m_ClassMembers;           // Members of current class
  std::string m_CurrentClassName;
  std::string m_CurrentMethodName;
  std::string m_CurrentMethodReturnType;
  bool m_HasReturn = false;

  // Validation methods
  void ValidateClass(std::shared_ptr<QClass> cls);
  void ValidateMethod(std::shared_ptr<QMethod> method,
                      const std::string &className);
  void ValidateCode(std::shared_ptr<QCode> code);
  void ValidateNode(std::shared_ptr<QNode> node);
  void ValidateExpression(std::shared_ptr<QExpression> expr);
  void ValidateVariableDecl(std::shared_ptr<QVariableDecl> varDecl);
  void ValidateAssign(std::shared_ptr<QAssign> assign);
  void ValidateMemberAssign(std::shared_ptr<QMemberAssign> memberAssign);
  void ValidateMethodCall(std::shared_ptr<QMethodCall> methodCall);
  void ValidateStatement(std::shared_ptr<QStatement> stmt);
  void ValidateIf(std::shared_ptr<QIf> ifNode);
  void ValidateFor(std::shared_ptr<QFor> forNode);
  void ValidateWhile(std::shared_ptr<QWhile> whileNode);
  void ValidateReturn(std::shared_ptr<QReturn> returnNode);

  // Helper methods
  bool IsValidTypeName(const std::string &typeName) const;
  bool IsKnownClass(const std::string &className) const;
  bool IsKnownVariable(const std::string &varName) const;
  void ReportError(const std::string &message, int line = 0);
  void ReportWarning(const std::string &message, int line = 0);
};
