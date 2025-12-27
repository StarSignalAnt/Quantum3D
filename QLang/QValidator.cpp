#include "QValidator.h"
#include "QAssign.h"
#include "QClass.h"
#include "QCode.h"
#include "QExpression.h"
#include "QFor.h"
#include "QIf.h"
#include "QInstanceDecl.h"
#include "QMemberAssign.h"
#include "QMethod.h"
#include "QMethodCall.h"
#include "QReturn.h"
#include "QStatement.h"
#include "QVariableDecl.h"
#include "QWhile.h"
#include "Tokenizer.h"
#include <iostream>

QValidator::QValidator(std::shared_ptr<QErrorCollector> errorCollector)
    : m_ErrorCollector(errorCollector) {
  if (!m_ErrorCollector) {
    m_ErrorCollector = std::make_shared<QErrorCollector>();
  }

  // Register built-in types as known "classes" for type checking
  m_KnownClasses.insert("Vec3");
  m_KnownClasses.insert("Matrix");
  m_KnownClasses.insert("GameNode");
}

bool QValidator::Validate(std::shared_ptr<QProgram> program) {
  if (!program) {
    ReportError("Null program passed to validator");
    return false;
  }

  std::cout << "[DEBUG] QValidator: Starting validation" << std::endl;

  // First pass: collect all declared classes and enums
  for (const auto &cls : program->GetClasses()) {
    if (cls) {
      m_DeclaredClasses.insert(cls->GetName());
    }
  }
  for (const auto &enumDef : program->GetEnums()) {
    if (enumDef) {
      m_DeclaredEnums.insert(enumDef->GetName());
    }
  }

  // Second pass: validate each class
  for (const auto &cls : program->GetClasses()) {
    if (cls) {
      ValidateClass(cls);
    }
  }

  // Validate global code
  if (program->GetCode()) {
    m_CurrentClassName = "";
    m_CurrentMethodName = "";
    m_CurrentScopeVars.clear();
    m_ClassMembers.clear();
    ValidateCode(program->GetCode());
  }

  bool hasErrors = m_ErrorCollector->HasErrors();
  if (hasErrors) {
    std::cerr << "[ERROR] QValidator: Validation failed with errors:"
              << std::endl;
    m_ErrorCollector->ListErrors();
  } else {
    std::cout << "[DEBUG] QValidator: Validation passed" << std::endl;
  }

  return !hasErrors;
}

void QValidator::RegisterKnownClass(const std::string &className) {
  m_KnownClasses.insert(className);
}

void QValidator::RegisterKnownClasses(const std::set<std::string> &classNames) {
  m_KnownClasses.insert(classNames.begin(), classNames.end());
}

void QValidator::ValidateClass(std::shared_ptr<QClass> cls) {
  if (!cls)
    return;

  m_CurrentClassName = cls->GetName();
  m_ClassMembers.clear();

  std::cout << "[DEBUG] QValidator: Validating class '" << m_CurrentClassName
            << "'" << std::endl;

  // Check parent class exists
  if (cls->HasParent()) {
    std::string parentName = cls->GetParentClassName();
    if (!IsKnownClass(parentName)) {
      ReportError("Parent class '" + parentName + "' not found for class '" +
                  m_CurrentClassName + "'");
    }
  }

  // Validate members
  for (const auto &member : cls->GetMembers()) {
    if (member) {
      std::string memberName = member->GetName();

      // Check for duplicate members
      if (m_ClassMembers.count(memberName)) {
        ReportError("Duplicate member '" + memberName + "' in class '" +
                    m_CurrentClassName + "'");
      }
      m_ClassMembers.insert(memberName);

      // Validate member type
      std::string typeName = member->GetTypeName();
      if (!typeName.empty() && !IsValidTypeName(typeName)) {
        // Check if it's a class type
        if (!IsKnownClass(typeName)) {
          ReportError("Unknown type '" + typeName + "' for member '" +
                      memberName + "' in class '" + m_CurrentClassName + "'");
        }
      }
    }
  }

  // Validate methods
  for (const auto &method : cls->GetMethods()) {
    if (method) {
      ValidateMethod(method, m_CurrentClassName);
    }
  }

  m_CurrentClassName = "";
}

void QValidator::ValidateMethod(std::shared_ptr<QMethod> method,
                                const std::string &className) {
  if (!method)
    return;

  m_CurrentMethodName = method->GetName();
  m_CurrentScopeVars.clear();
  m_HasReturn = false;

  std::cout << "[DEBUG] QValidator: Validating method '" << className << "."
            << m_CurrentMethodName << "'" << std::endl;

  // Get return type
  TokenType returnType = method->GetReturnType();
  std::string returnTypeName = method->GetReturnTypeName();
  bool isVoid = (returnType == TokenType::T_VOID || returnTypeName == "void");
  m_CurrentMethodReturnType = returnTypeName;

  // Validate return type
  if (!returnTypeName.empty() && returnTypeName != "void") {
    if (!IsValidTypeName(returnTypeName) && !IsKnownClass(returnTypeName)) {
      ReportError("Unknown return type '" + returnTypeName + "' for method '" +
                  m_CurrentMethodName + "'");
    }
  }

  // Validate parameters
  std::set<std::string> paramNames;
  for (const auto &param : method->GetParameters()) {
    // Check for duplicate parameters
    if (paramNames.count(param.name)) {
      ReportError("Duplicate parameter '" + param.name + "' in method '" +
                  m_CurrentMethodName + "'");
    }
    paramNames.insert(param.name);
    m_CurrentScopeVars.insert(param.name);

    // Validate parameter type
    if (!param.typeName.empty()) {
      if (!IsValidTypeName(param.typeName) && !IsKnownClass(param.typeName)) {
        ReportError("Unknown type '" + param.typeName + "' for parameter '" +
                    param.name + "' in method '" + m_CurrentMethodName + "'");
      }
    }
  }

  // Validate method body
  if (method->GetBody()) {
    ValidateCode(method->GetBody());
  }

  // Check for missing return in non-void methods
  if (!isVoid && !m_HasReturn && m_CurrentMethodName != className) {
    // Constructor doesn't need return
    ReportWarning("Method '" + m_CurrentMethodName +
                  "' may not return a value on all paths");
  }

  m_CurrentMethodName = "";
}

void QValidator::ValidateCode(std::shared_ptr<QCode> code) {
  if (!code)
    return;

  for (const auto &node : code->GetNodes()) {
    if (node) {
      ValidateNode(node);
    }
  }
}

void QValidator::ValidateNode(std::shared_ptr<QNode> node) {
  if (!node)
    return;

  // Try each node type
  if (auto varDecl = std::dynamic_pointer_cast<QVariableDecl>(node)) {
    ValidateVariableDecl(varDecl);
  } else if (auto assign = std::dynamic_pointer_cast<QAssign>(node)) {
    ValidateAssign(assign);
  } else if (auto memberAssign =
                 std::dynamic_pointer_cast<QMemberAssign>(node)) {
    ValidateMemberAssign(memberAssign);
  } else if (auto methodCall = std::dynamic_pointer_cast<QMethodCall>(node)) {
    ValidateMethodCall(methodCall);
  } else if (auto stmt = std::dynamic_pointer_cast<QStatement>(node)) {
    ValidateStatement(stmt);
  } else if (auto ifNode = std::dynamic_pointer_cast<QIf>(node)) {
    ValidateIf(ifNode);
  } else if (auto forNode = std::dynamic_pointer_cast<QFor>(node)) {
    ValidateFor(forNode);
  } else if (auto whileNode = std::dynamic_pointer_cast<QWhile>(node)) {
    ValidateWhile(whileNode);
  } else if (auto returnNode = std::dynamic_pointer_cast<QReturn>(node)) {
    ValidateReturn(returnNode);
  } else if (auto instDecl = std::dynamic_pointer_cast<QInstanceDecl>(node)) {
    // Instance declaration - validate class exists
    std::string className = instDecl->GetQClassName();
    if (!IsKnownClass(className)) {
      ReportError("Unknown class type '" + className +
                  "' in instance declaration");
    }
    m_CurrentScopeVars.insert(instDecl->GetInstanceName());
  }
}

void QValidator::ValidateExpression(std::shared_ptr<QExpression> expr) {
  if (!expr)
    return;

  const auto &elements = expr->GetElements();
  for (size_t i = 0; i < elements.size(); i++) {
    const Token &token = elements[i];

    if (token.type == TokenType::T_IDENTIFIER) {
      std::string name = token.value;

      // Check if it's a known variable, class member, or class name
      if (!IsKnownVariable(name) && !m_ClassMembers.count(name) &&
          !IsKnownClass(name) && !m_DeclaredEnums.count(name)) {

        // Check if it's a member access (next token is .)
        bool isMemberAccess = (i + 1 < elements.size() &&
                               elements[i + 1].type == TokenType::T_DOT);
        bool isMethodCall = (i + 1 < elements.size() &&
                             elements[i + 1].type == TokenType::T_LPAREN);

        // Only report error for standalone identifiers
        if (!isMemberAccess && !isMethodCall) {
          // It might be an implicit this.member access, allow it
          // ReportWarning("Possibly undefined variable: '" + name + "'",
          // token.line);
        }
      }
    }
  }
}

void QValidator::ValidateVariableDecl(std::shared_ptr<QVariableDecl> varDecl) {
  if (!varDecl)
    return;

  std::string varName = varDecl->GetName();
  std::string typeName = varDecl->GetTypeName();

  // Check for duplicate variable
  if (m_CurrentScopeVars.count(varName)) {
    ReportWarning("Variable '" + varName + "' shadows existing declaration");
  }
  m_CurrentScopeVars.insert(varName);

  // Validate type
  if (!typeName.empty()) {
    if (!IsValidTypeName(typeName) && !IsKnownClass(typeName)) {
      ReportError("Unknown type '" + typeName + "' for variable '" + varName +
                  "'");
    }
  }

  // Validate initializer expression
  if (varDecl->HasInitializer()) {
    ValidateExpression(varDecl->GetInitializer());
  }
}

void QValidator::ValidateAssign(std::shared_ptr<QAssign> assign) {
  if (!assign)
    return;

  std::string varName = assign->GetVariableName();

  // Check if variable exists
  if (!IsKnownVariable(varName) && !m_ClassMembers.count(varName)) {
    ReportError("Assignment to undefined variable: '" + varName + "'");
  }

  // Validate value expression
  if (assign->GetValueExpression()) {
    ValidateExpression(assign->GetValueExpression());
  }
}

void QValidator::ValidateMemberAssign(
    std::shared_ptr<QMemberAssign> memberAssign) {
  if (!memberAssign)
    return;

  std::string instanceName = memberAssign->GetInstanceName();

  // Check if instance exists (unless it's 'this')
  if (instanceName != "this" && !IsKnownVariable(instanceName) &&
      !m_ClassMembers.count(instanceName)) {
    ReportError("Member access on undefined instance: '" + instanceName + "'");
  }

  // Validate value expression
  if (memberAssign->GetValueExpression()) {
    ValidateExpression(memberAssign->GetValueExpression());
  }
}

void QValidator::ValidateMethodCall(std::shared_ptr<QMethodCall> methodCall) {
  if (!methodCall)
    return;

  std::string instanceName = methodCall->GetInstanceName();

  // Check if instance exists (unless it's 'this' or empty for implicit this)
  if (!instanceName.empty() && instanceName != "this" &&
      !IsKnownVariable(instanceName) && !m_ClassMembers.count(instanceName)) {
    ReportError("Method call on undefined instance: '" + instanceName + "'");
  }

  // Validate arguments
  if (auto args = methodCall->GetArguments()) {
    for (const auto &arg : args->GetParameters()) {
      ValidateExpression(arg);
    }
  }
}

void QValidator::ValidateStatement(std::shared_ptr<QStatement> stmt) {
  if (!stmt)
    return;

  // Validate arguments
  if (auto params = stmt->GetParameters()) {
    for (const auto &param : params->GetParameters()) {
      ValidateExpression(param);
    }
  }
}

void QValidator::ValidateIf(std::shared_ptr<QIf> ifNode) {
  if (!ifNode)
    return;

  // Validate condition
  ValidateExpression(ifNode->GetCondition());

  // Validate then block
  if (ifNode->GetThenBlock()) {
    ValidateCode(ifNode->GetThenBlock());
  }

  // Validate else-if blocks
  for (const auto &elseIfPair : ifNode->GetElseIfBlocks()) {
    ValidateExpression(elseIfPair.first);
    if (elseIfPair.second) {
      ValidateCode(elseIfPair.second);
    }
  }

  // Validate else block
  if (ifNode->HasElse() && ifNode->GetElseBlock()) {
    ValidateCode(ifNode->GetElseBlock());
  }
}

void QValidator::ValidateFor(std::shared_ptr<QFor> forNode) {
  if (!forNode)
    return;

  // Add loop variable to scope
  std::string loopVar = forNode->GetVarName();
  m_CurrentScopeVars.insert(loopVar);

  // Validate start/end/step expressions
  ValidateExpression(forNode->GetStart());
  ValidateExpression(forNode->GetEnd());
  if (forNode->GetStep()) {
    ValidateExpression(forNode->GetStep());
  }

  // Validate body
  if (forNode->GetBody()) {
    ValidateCode(forNode->GetBody());
  }
}

void QValidator::ValidateWhile(std::shared_ptr<QWhile> whileNode) {
  if (!whileNode)
    return;

  // Validate condition
  ValidateExpression(whileNode->GetCondition());

  // Validate body
  if (whileNode->GetBody()) {
    ValidateCode(whileNode->GetBody());
  }
}

void QValidator::ValidateReturn(std::shared_ptr<QReturn> returnNode) {
  if (!returnNode)
    return;

  m_HasReturn = true;

  // Validate return expression
  if (returnNode->HasExpression()) {
    ValidateExpression(returnNode->GetExpression());
  }
}

bool QValidator::IsValidTypeName(const std::string &typeName) const {
  // Valid primitive types in QLang
  static const std::set<std::string> validTypes = {
      "int32", "int64", "float32", "float64", "bool", "string",
      "byte",  "iptr",  "fptr",    "bptr",    "ptr",  "void"};

  // Check if it's a valid primitive type
  if (validTypes.count(typeName)) {
    return true;
  }

  // Check for INVALID shorthand types (these are common mistakes)
  static const std::set<std::string> invalidTypes = {"int",  "float", "double",
                                                     "long", "short", "char"};

  if (invalidTypes.count(typeName)) {
    return false; // This will trigger an error for illegal types
  }

  return false; // Unknown type - caller should check if it's a class
}

bool QValidator::IsKnownClass(const std::string &className) const {
  return m_KnownClasses.count(className) > 0 ||
         m_DeclaredClasses.count(className) > 0;
}

bool QValidator::IsKnownVariable(const std::string &varName) const {
  return m_CurrentScopeVars.count(varName) > 0;
}

void QValidator::ReportError(const std::string &message, int line) {
  std::string context;
  if (!m_CurrentClassName.empty()) {
    context = m_CurrentClassName;
    if (!m_CurrentMethodName.empty()) {
      context += "." + m_CurrentMethodName;
    }
  }

  m_ErrorCollector->ReportError(QErrorSeverity::Error, message, line, 0, 0,
                                "validator", context);
}

void QValidator::ReportWarning(const std::string &message, int line) {
  std::string context;
  if (!m_CurrentClassName.empty()) {
    context = m_CurrentClassName;
    if (!m_CurrentMethodName.empty()) {
      context += "." + m_CurrentMethodName;
    }
  }

  m_ErrorCollector->ReportError(QErrorSeverity::Warning, message, line, 0, 0,
                                "validator", context);
}
