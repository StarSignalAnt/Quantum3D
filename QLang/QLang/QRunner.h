#pragma once

#include "Parser.h"
#include "QClassInstance.h"
#include "QContext.h"
#include "QInstanceDecl.h"
#include "QMemberAssign.h"
#include "QMethodCall.h"
#include "QReturn.h"
#include <iostream>
#include <memory>
#include <unordered_map>

// QRunner - executes a parsed QProgram
class QRunner {
public:
  QRunner(std::shared_ptr<QContext> context) : m_Context(context) {
    std::cout << "[DEBUG] QRunner created" << std::endl;
  }

  ~QRunner() { std::cout << "[DEBUG] QRunner destroyed" << std::endl; }

  // Run a program
  void Run(std::shared_ptr<QProgram> program) {
    std::cout << "[DEBUG] QRunner::Run() - starting execution" << std::endl;

    // Register class definitions from the program
    for (const auto &cls : program->GetClasses()) {
      m_Classes[cls->GetName()] = cls;
      std::cout << "[DEBUG] QRunner::Run() - registered class: "
                << cls->GetName() << std::endl;
    }

    auto code = program->GetCode();
    ExecuteCode(code);

    std::cout << "[DEBUG] QRunner::Run() - execution complete" << std::endl;
  }

private:
  std::shared_ptr<QContext> m_Context;
  std::unordered_map<std::string, std::shared_ptr<QClass>> m_Classes;
  QValue m_ReturnValue = std::monostate{};
  bool m_HasReturn = false;

  // Execute a QCode block
  void ExecuteCode(std::shared_ptr<QCode> code) {
    std::cout << "[DEBUG] QRunner::ExecuteCode() - executing code block"
              << std::endl;

    const auto &nodes = code->GetNodes();
    for (const auto &node : nodes) {
      ExecuteNode(node);
    }
  }

  // Execute any QNode (dispatch to appropriate handler)
  void ExecuteNode(std::shared_ptr<QNode> node) {
    // Check if it's a variable declaration
    auto varDecl = std::dynamic_pointer_cast<QVariableDecl>(node);
    if (varDecl) {
      ExecuteVariableDecl(varDecl);
      return;
    }

    // Check if it's an instance declaration
    auto instanceDecl = std::dynamic_pointer_cast<QInstanceDecl>(node);
    if (instanceDecl) {
      ExecuteInstanceDecl(instanceDecl);
      return;
    }

    // Check if it's a statement
    auto statement = std::dynamic_pointer_cast<QStatement>(node);
    if (statement) {
      ExecuteStatement(statement);
      return;
    }

    // Check if it's a method call
    auto methodCall = std::dynamic_pointer_cast<QMethodCall>(node);
    if (methodCall) {
      ExecuteMethodCall(methodCall);
      return;
    }

    // Check if it's a member assignment
    auto memberAssign = std::dynamic_pointer_cast<QMemberAssign>(node);
    if (memberAssign) {
      ExecuteMemberAssign(memberAssign);
      return;
    }

    // Check if it's a return statement
    auto returnStmt = std::dynamic_pointer_cast<QReturn>(node);
    if (returnStmt) {
      ExecuteReturn(returnStmt);
      return;
    }

    std::cout << "[DEBUG] QRunner::ExecuteNode() - unknown node type: "
              << node->GetName() << std::endl;
  }

  // Execute a variable declaration
  void ExecuteVariableDecl(std::shared_ptr<QVariableDecl> varDecl) {
    std::string name = varDecl->GetName();
    TokenType varType = varDecl->GetVarType();

    std::cout << "[DEBUG] QRunner::ExecuteVariableDecl() - declaring: " << name
              << std::endl;

    QValue value;

    // If there's an initializer, evaluate it
    if (varDecl->HasInitializer()) {
      value = EvaluateExpression(varDecl->GetInitializer());

      // Coerce to declared type
      value = CoerceToType(value, varType);
    } else {
      // Default value based on type
      value = GetDefaultValue(varType);
    }

    // Store in context
    m_Context->SetVariable(name, value);
  }

  // Execute an instance declaration (e.g., Test t1 = new Test();)
  void ExecuteInstanceDecl(std::shared_ptr<QInstanceDecl> instanceDecl) {
    std::string className = instanceDecl->GetClassName();
    std::string instanceName = instanceDecl->GetInstanceName();

    std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - creating instance: "
              << className << " " << instanceName << std::endl;

    // Look up the class definition
    auto classIt = m_Classes.find(className);
    if (classIt == m_Classes.end()) {
      std::cerr << "[ERROR] QRunner::ExecuteInstanceDecl() - class not found: "
                << className << std::endl;
      return;
    }

    std::shared_ptr<QClass> classDef = classIt->second;

    // Create a new instance of the class
    auto instance = std::make_shared<QClassInstance>(classDef);

    // Initialize member variables with their default expressions
    InitializeInstanceMembers(instance, classDef);

    // Find and execute constructor if exists
    for (const auto &method : classDef->GetMethods()) {
      if (method->GetName() == className) {
        std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - executing "
                     "constructor for: "
                  << className << std::endl;

        // Execute constructor with instance context
        ExecuteMethod(method, instance);
        break;
      }
    }

    // Store the instance in the context
    m_Context->SetVariable(instanceName, instance);

    std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - instance created: "
              << instanceName << std::endl;
  }

  // Execute a method call on an instance (e.g., t1.TestMeth() or t1.ot.Value())
  void ExecuteMethodCall(std::shared_ptr<QMethodCall> methodCall) {
    std::string instancePath = methodCall->GetInstanceName();
    std::string methodName = methodCall->GetMethodName();

    std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - calling: "
              << instancePath << "." << methodName << "()" << std::endl;

    // Split instance path by dots for chained access
    std::vector<std::string> pathParts;
    std::string current;
    for (char c : instancePath) {
      if (c == '.') {
        if (!current.empty()) {
          pathParts.push_back(current);
          current.clear();
        }
      } else {
        current += c;
      }
    }
    if (!current.empty()) {
      pathParts.push_back(current);
    }

    // Look up the first instance in path
    std::string firstName = pathParts[0];
    QValue instanceVal = m_Context->GetVariable(firstName);

    // Check if it's a class instance
    if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(instanceVal)) {
      std::cerr << "[ERROR] QRunner::ExecuteMethodCall() - '" << firstName
                << "' is not a class instance" << std::endl;
      return;
    }

    auto currentInstance =
        std::get<std::shared_ptr<QClassInstance>>(instanceVal);

    // Traverse remaining path parts to get to final instance
    for (size_t i = 1; i < pathParts.size(); i++) {
      std::string nestedName = pathParts[i];
      std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - traversing: "
                << nestedName << std::endl;

      if (currentInstance->HasNestedInstance(nestedName)) {
        currentInstance = currentInstance->GetNestedInstance(nestedName);
      } else {
        std::cerr << "[ERROR] QRunner::ExecuteMethodCall() - nested instance '"
                  << nestedName << "' not found" << std::endl;
        return;
      }
    }

    auto classDef = currentInstance->GetClassDef();

    // Find the method by name
    std::shared_ptr<QMethod> targetMethod = nullptr;
    for (const auto &method : classDef->GetMethods()) {
      if (method->GetName() == methodName) {
        targetMethod = method;
        break;
      }
    }

    if (!targetMethod) {
      std::cerr << "[ERROR] QRunner::ExecuteMethodCall() - method '"
                << methodName << "' not found in class '" << classDef->GetName()
                << "'" << std::endl;
      return;
    }

    std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - found method: "
              << methodName << std::endl;

    // Evaluate arguments (back-to-front as per language convention)
    std::vector<QValue> argValues;
    auto args = methodCall->GetArguments();
    if (args) {
      const auto &params = args->GetParameters();
      std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - evaluating "
                << params.size() << " arguments" << std::endl;

      // Evaluate back-to-front
      for (int i = static_cast<int>(params.size()) - 1; i >= 0; i--) {
        QValue val = EvaluateExpression(params[i]);
        argValues.insert(argValues.begin(), val);
        std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - arg[" << i
                  << "] = " << ValueToString(val) << std::endl;
      }
    }

    // Execute the method with the instance context and arguments
    ExecuteMethod(targetMethod, currentInstance, argValues);

    std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - method call complete"
              << std::endl;
  }

  // Execute a member field assignment (e.g., t1.num = 90; or t1.ot.check = 50;)
  void ExecuteMemberAssign(std::shared_ptr<QMemberAssign> memberAssign) {
    std::string instanceName = memberAssign->GetInstanceName();
    std::string memberPath = memberAssign->GetMemberName();

    std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - assigning: "
              << instanceName << "." << memberPath << std::endl;

    // Split member path by dots for chained access (e.g., "ot.check" -> ["ot",
    // "check"])
    std::vector<std::string> pathParts;
    std::string current;
    for (char c : memberPath) {
      if (c == '.') {
        if (!current.empty()) {
          pathParts.push_back(current);
          current.clear();
        }
      } else {
        current += c;
      }
    }
    if (!current.empty()) {
      pathParts.push_back(current);
    }

    // Look up the initial instance variable
    QValue instanceVal = m_Context->GetVariable(instanceName);

    // Check if it's a class instance
    if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(instanceVal)) {
      std::cerr << "[ERROR] QRunner::ExecuteMemberAssign() - '" << instanceName
                << "' is not a class instance" << std::endl;
      return;
    }

    auto currentInstance =
        std::get<std::shared_ptr<QClassInstance>>(instanceVal);

    // Traverse the chain (all but the last part)
    for (size_t i = 0; i < pathParts.size() - 1; i++) {
      std::string nestedName = pathParts[i];
      std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - traversing: "
                << nestedName << std::endl;

      // Check if this is a nested instance
      if (currentInstance->HasNestedInstance(nestedName)) {
        currentInstance = currentInstance->GetNestedInstance(nestedName);
      } else {
        std::cerr
            << "[ERROR] QRunner::ExecuteMemberAssign() - nested instance '"
            << nestedName << "' not found" << std::endl;
        return;
      }
    }

    // The last part is the member to set
    std::string finalMember = pathParts.back();

    // Check if the member exists
    if (!currentInstance->HasMember(finalMember)) {
      std::cerr << "[ERROR] QRunner::ExecuteMemberAssign() - member '"
                << finalMember << "' not found in instance" << std::endl;
      return;
    }

    // Evaluate the value expression
    QValue newValue = EvaluateExpression(memberAssign->GetValueExpression());

    // Convert to QInstanceValue and set on the instance
    QInstanceValue instVal = ConvertQValueToInstanceValue(newValue);
    currentInstance->SetMember(finalMember, instVal);

    std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - set " << finalMember
              << " = " << ValueToString(newValue) << std::endl;
  }

  // Execute a return statement
  void ExecuteReturn(std::shared_ptr<QReturn> returnStmt) {
    std::cout << "[DEBUG] QRunner::ExecuteReturn() - executing return"
              << std::endl;

    if (returnStmt->HasExpression()) {
      m_ReturnValue = EvaluateExpression(returnStmt->GetExpression());
      std::cout << "[DEBUG] QRunner::ExecuteReturn() - return value: "
                << ValueToString(m_ReturnValue) << std::endl;
    } else {
      m_ReturnValue = std::monostate{};
    }

    m_HasReturn = true;
  }

  // Check if a return value is pending
  bool HasReturnValue() const { return m_HasReturn; }

  // Get the return value and clear the flag
  QValue GetReturnValue() {
    m_HasReturn = false;
    return m_ReturnValue;
  }

  // Execute a method with an instance context and optional arguments
  void ExecuteMethod(std::shared_ptr<QMethod> method,
                     std::shared_ptr<QClassInstance> instance,
                     const std::vector<QValue> &args = {}) {
    std::cout << "[DEBUG] QRunner::ExecuteMethod() - executing method: "
              << method->GetName() << std::endl;

    // Create a child context for the method execution
    // This context will have access to instance members as local variables
    auto methodContext =
        std::make_shared<QContext>("method:" + method->GetName(), m_Context);

    // Copy instance member variables into the method's local context
    for (const auto &[memberName, memberValue] : instance->GetMembers()) {
      // Convert QInstanceValue to QValue
      QValue qval = ConvertInstanceValueToQValue(memberValue);
      methodContext->SetVariable(memberName, qval);
      std::cout << "[DEBUG] QRunner::ExecuteMethod() - loaded member: "
                << memberName << std::endl;
    }

    // Also load nested class instances into the method context
    // This allows access to nested instance members like ot.check
    for (const auto &nestedName : instance->GetNestedInstanceNames()) {
      auto nestedInstance = instance->GetNestedInstance(nestedName);
      if (nestedInstance) {
        methodContext->SetVariable(nestedName, nestedInstance);
        std::cout
            << "[DEBUG] QRunner::ExecuteMethod() - loaded nested instance: "
            << nestedName << std::endl;
      }
    }

    // Bind arguments to parameters
    const auto &params = method->GetParameters();
    for (size_t i = 0; i < params.size() && i < args.size(); i++) {
      methodContext->SetVariable(params[i].name, args[i]);
      std::cout << "[DEBUG] QRunner::ExecuteMethod() - bound param "
                << params[i].name << " = " << ValueToString(args[i])
                << std::endl;
    }

    // Save current context and switch to method context
    auto savedContext = m_Context;
    m_Context = methodContext;

    // Execute the method body
    ExecuteCode(method->GetBody());

    // Copy modified values back to the instance
    for (const auto &[memberName, memberValue] : instance->GetMembers()) {
      if (methodContext->HasLocalVariable(memberName)) {
        QValue newValue = methodContext->GetVariable(memberName);
        QInstanceValue instVal = ConvertQValueToInstanceValue(newValue);
        instance->SetMember(memberName, instVal);
        std::cout << "[DEBUG] QRunner::ExecuteMethod() - updated member: "
                  << memberName << std::endl;
      }
    }

    // Restore original context
    m_Context = savedContext;

    std::cout << "[DEBUG] QRunner::ExecuteMethod() - method complete: "
              << method->GetName() << std::endl;
  }

  // Convert QInstanceValue to QValue
  QValue ConvertInstanceValueToQValue(const QInstanceValue &instVal) {
    if (std::holds_alternative<std::monostate>(instVal))
      return std::monostate{};
    if (std::holds_alternative<bool>(instVal))
      return std::get<bool>(instVal);
    if (std::holds_alternative<int32_t>(instVal))
      return std::get<int32_t>(instVal);
    if (std::holds_alternative<int64_t>(instVal))
      return std::get<int64_t>(instVal);
    if (std::holds_alternative<float>(instVal))
      return std::get<float>(instVal);
    if (std::holds_alternative<double>(instVal))
      return std::get<double>(instVal);
    if (std::holds_alternative<std::string>(instVal))
      return std::get<std::string>(instVal);
    return std::monostate{};
  }

  // Convert QValue to QInstanceValue
  QInstanceValue ConvertQValueToInstanceValue(const QValue &qval) {
    if (std::holds_alternative<std::monostate>(qval))
      return std::monostate{};
    if (std::holds_alternative<bool>(qval))
      return std::get<bool>(qval);
    if (std::holds_alternative<int32_t>(qval))
      return std::get<int32_t>(qval);
    if (std::holds_alternative<int64_t>(qval))
      return std::get<int64_t>(qval);
    if (std::holds_alternative<float>(qval))
      return std::get<float>(qval);
    if (std::holds_alternative<double>(qval))
      return std::get<double>(qval);
    if (std::holds_alternative<std::string>(qval))
      return std::get<std::string>(qval);
    // Class instances stay as they are in the parent scope
    return std::monostate{};
  }

  // Initialize instance members with their default expressions from the class
  void InitializeInstanceMembers(std::shared_ptr<QClassInstance> instance,
                                 std::shared_ptr<QClass> classDef) {
    std::cout << "[DEBUG] QRunner::InitializeInstanceMembers() - initializing "
                 "members for: "
              << classDef->GetName() << std::endl;

    for (const auto &member : classDef->GetMembers()) {
      std::string memberName = member->GetName();
      TokenType memberType = member->GetVarType();

      QValue value;

      // Check if this is a class instance member (non-primitive type)
      // T_IDENTIFIER means it's a class type
      if (memberType == TokenType::T_IDENTIFIER && member->HasInitializer()) {
        const auto &initExpr = member->GetInitializer();
        const auto &elements = initExpr->GetElements();

        // Check for "new ClassName()" pattern
        // Pattern: [new] [ClassName] [(] [)] or [new] [ClassName] [(] args...
        // [)]
        if (elements.size() >= 3 && elements[0].type == TokenType::T_NEW &&
            elements[1].type == TokenType::T_IDENTIFIER) {
          std::string nestedClassName = elements[1].value;

          std::cout << "[DEBUG] QRunner::InitializeInstanceMembers() - "
                       "creating nested instance: "
                    << nestedClassName << std::endl;

          // Look up the nested class definition
          auto classIt = m_Classes.find(nestedClassName);
          if (classIt == m_Classes.end()) {
            std::cerr << "[ERROR] InitializeInstanceMembers() - class not "
                         "found: "
                      << nestedClassName << std::endl;
            continue;
          }

          std::shared_ptr<QClass> nestedClassDef = classIt->second;

          // Create nested instance
          auto nestedInstance =
              std::make_shared<QClassInstance>(nestedClassDef);

          // Initialize nested instance's members (recursive)
          InitializeInstanceMembers(nestedInstance, nestedClassDef);

          // Find and call constructor if exists
          for (const auto &method : nestedClassDef->GetMethods()) {
            if (method->GetName() == nestedClassName) {
              std::cout << "[DEBUG] InitializeInstanceMembers() - executing "
                           "nested constructor: "
                        << nestedClassName << std::endl;
              ExecuteMethod(method, nestedInstance);
              break;
            }
          }

          // Store nested instance as this member's value
          // Note: We store as QValue (shared_ptr<QClassInstance>) directly
          value = nestedInstance;

          std::cout << "[DEBUG] InitializeInstanceMembers() - nested instance "
                       "created: "
                    << memberName << std::endl;
        } else {
          // Unknown initializer pattern for class type
          std::cerr << "[ERROR] InitializeInstanceMembers() - unknown "
                       "initializer for class member: "
                    << memberName << std::endl;
          value = std::monostate{};
        }
      } else if (member->HasInitializer()) {
        // Primitive type with initializer - evaluate expression
        std::cout << "[DEBUG] QRunner::InitializeInstanceMembers() - "
                     "evaluating initializer for: "
                  << memberName << std::endl;
        value = EvaluateExpression(member->GetInitializer());
        // Coerce to the declared type
        value = CoerceToType(value, memberType);
      } else {
        // Use default value for the type
        value = GetDefaultValue(memberType);
      }

      // Store the value
      // For class instances, we need to store differently
      if (std::holds_alternative<std::shared_ptr<QClassInstance>>(value)) {
        // Store as QValue directly - this is a nested instance reference
        // We'll need to extend instance storage to handle this case
        // For now, store the instance name reference
        std::cout << "[DEBUG] InitializeInstanceMembers() - storing nested "
                     "instance reference for: "
                  << memberName << std::endl;
        // Note: QInstanceValue can't hold shared_ptr<QClassInstance>
        // We'll store it in a separate map in QClassInstance
        instance->SetNestedInstance(
            memberName, std::get<std::shared_ptr<QClassInstance>>(value));
      } else {
        // Convert QValue to QInstanceValue and set on instance
        QInstanceValue instVal = ConvertQValueToInstanceValue(value);
        instance->SetMember(memberName, instVal);

        std::cout << "[DEBUG] QRunner::InitializeInstanceMembers() - set "
                  << memberName << " = " << ValueToString(value) << std::endl;
      }
    }
  }

  // Get default value for a type
  QValue GetDefaultValue(TokenType type) {
    switch (type) {
    case TokenType::T_INT32:
      return static_cast<int32_t>(0);
    case TokenType::T_INT64:
      return static_cast<int64_t>(0);
    case TokenType::T_FLOAT32:
      return 0.0f;
    case TokenType::T_FLOAT64:
      return 0.0;
    case TokenType::T_SHORT:
      return static_cast<int32_t>(0);
    case TokenType::T_STRING_TYPE:
      return std::string("");
    case TokenType::T_BOOL:
      return false;
    default:
      return std::monostate{};
    }
  }

  // Coerce a value to a specific type
  QValue CoerceToType(const QValue &value, TokenType targetType) {
    switch (targetType) {
    case TokenType::T_INT32:
      return static_cast<int32_t>(ToInt64(value));
    case TokenType::T_INT64:
      return ToInt64(value);
    case TokenType::T_FLOAT32:
      return static_cast<float>(ToDouble(value));
    case TokenType::T_FLOAT64:
      return ToDouble(value);
    case TokenType::T_STRING_TYPE:
      if (std::holds_alternative<std::string>(value)) {
        return value;
      }
      return ValueToString(value);
    case TokenType::T_BOOL:
      if (std::holds_alternative<bool>(value)) {
        return value;
      }
      // Truthy check
      if (std::holds_alternative<int32_t>(value))
        return std::get<int32_t>(value) != 0;
      if (std::holds_alternative<int64_t>(value))
        return std::get<int64_t>(value) != 0;
      return false;
    default:
      return value;
    }
  }

  // Execute a statement
  void ExecuteStatement(std::shared_ptr<QStatement> statement) {
    std::string funcName = statement->GetName();
    std::cout << "[DEBUG] QRunner::ExecuteStatement() - executing: " << funcName
              << std::endl;

    // Build arguments from parameters
    std::vector<QValue> args;
    auto params = statement->GetParameters();
    if (params) {
      for (const auto &expr : params->GetParameters()) {
        QValue value = EvaluateExpression(expr);
        args.push_back(value);
      }
    }

    // Check if this is a registered function
    if (m_Context->HasFunc(funcName)) {
      QValue result = m_Context->CallFunc(funcName, args);
      std::cout << "[DEBUG] QRunner::ExecuteStatement() - function returned: "
                << ValueToString(result) << std::endl;
    } else {
      std::cerr << "[ERROR] QRunner::ExecuteStatement() - unknown function: "
                << funcName << std::endl;
    }
  }

  // Get operator precedence (higher = binds tighter)
  int GetPrecedence(const std::string &op) {
    if (op == "*" || op == "/")
      return 2;
    if (op == "+" || op == "-")
      return 1;
    return 0;
  }

  // Check if operator is left-associative
  bool IsLeftAssociative(const std::string &op) {
    return true; // All our operators are left-associative
  }

  // Preprocess expression tokens to combine member access and method call
  // patterns. Supports arbitrary depth like t1.ot.check or t1.ot.GetValue()
  std::vector<Token>
  PreprocessMemberAccess(const std::vector<Token> &elements) {
    std::vector<Token> result;

    for (size_t i = 0; i < elements.size(); i++) {
      // Check if this starts a potential chain (identifier followed by dot)
      if (elements[i].type == TokenType::T_IDENTIFIER &&
          i + 1 < elements.size() && elements[i + 1].type == TokenType::T_DOT) {
        // Build the full chain by consuming .identifier patterns
        std::string chain = elements[i].value;
        size_t j = i + 1;

        while (j + 1 < elements.size() &&
               elements[j].type == TokenType::T_DOT &&
               elements[j + 1].type == TokenType::T_IDENTIFIER) {
          chain += "." + elements[j + 1].value;
          j += 2; // Skip dot and identifier
        }

        // Check if this ends with () - method call
        if (j + 1 < elements.size() &&
            elements[j].type == TokenType::T_LPAREN &&
            elements[j + 1].type == TokenType::T_RPAREN) {
          // Method call
          Token methodCall;
          methodCall.type = TokenType::T_IDENTIFIER;
          methodCall.value = chain + "()";
          methodCall.line = elements[i].line;
          result.push_back(methodCall);
          i = j + 1; // Skip past the ()
          std::cout << "[DEBUG] PreprocessMemberAccess() - method call: "
                    << methodCall.value << std::endl;
        } else {
          // Member access
          Token memberAccess;
          memberAccess.type = TokenType::T_IDENTIFIER;
          memberAccess.value = chain;
          memberAccess.line = elements[i].line;
          result.push_back(memberAccess);
          i = j - 1; // Position at last consumed token
          std::cout << "[DEBUG] PreprocessMemberAccess() - combined: "
                    << memberAccess.value << std::endl;
        }
      } else {
        result.push_back(elements[i]);
      }
    }

    return result;
  }

  // Evaluate an expression using Shunting Yard algorithm for operator
  // precedence
  QValue EvaluateExpression(std::shared_ptr<QExpression> expr) {
    const auto &rawElements = expr->GetElements();

    if (rawElements.empty()) {
      return std::monostate{};
    }

    // Preprocess to combine member access patterns (t1.num -> single token)
    std::vector<Token> elements = PreprocessMemberAccess(rawElements);

    // Single token - just return its value
    if (elements.size() == 1) {
      return TokenToValue(elements[0]);
    }

    std::cout << "[DEBUG] EvaluateExpression() - using Shunting Yard RPN"
              << std::endl;

    // Shunting Yard: convert infix to RPN
    std::vector<Token> outputQueue;
    std::vector<Token> operatorStack;

    for (const auto &token : elements) {
      if (token.type == TokenType::T_LPAREN) {
        // Left paren - push to operator stack
        operatorStack.push_back(token);
      } else if (token.type == TokenType::T_RPAREN) {
        // Right paren - pop until left paren
        while (!operatorStack.empty() &&
               operatorStack.back().type != TokenType::T_LPAREN) {
          outputQueue.push_back(operatorStack.back());
          operatorStack.pop_back();
        }
        // Pop the left paren (don't add to output)
        if (!operatorStack.empty() &&
            operatorStack.back().type == TokenType::T_LPAREN) {
          operatorStack.pop_back();
        } else {
          std::cerr << "[ERROR] Mismatched parentheses" << std::endl;
        }
      } else if (token.type == TokenType::T_OPERATOR) {
        // Pop operators with higher/equal precedence from stack to output
        while (!operatorStack.empty() &&
               operatorStack.back().type != TokenType::T_LPAREN) {
          const auto &top = operatorStack.back();
          int topPrec = GetPrecedence(top.value);
          int curPrec = GetPrecedence(token.value);

          if ((IsLeftAssociative(token.value) && curPrec <= topPrec) ||
              (!IsLeftAssociative(token.value) && curPrec < topPrec)) {
            outputQueue.push_back(top);
            operatorStack.pop_back();
          } else {
            break;
          }
        }
        operatorStack.push_back(token);
      } else {
        // Operand - add to output queue
        outputQueue.push_back(token);
      }
    }

    // Pop remaining operators to output
    while (!operatorStack.empty()) {
      outputQueue.push_back(operatorStack.back());
      operatorStack.pop_back();
    }

    // Debug: print RPN
    std::cout << "[DEBUG] RPN: ";
    for (const auto &t : outputQueue) {
      std::cout << t.value << " ";
    }
    std::cout << std::endl;

    // Evaluate RPN
    std::vector<QValue> valueStack;

    for (const auto &token : outputQueue) {
      if (token.type == TokenType::T_OPERATOR) {
        // Pop two operands
        if (valueStack.size() < 2) {
          std::cerr << "[ERROR] Not enough operands for operator: "
                    << token.value << std::endl;
          return std::monostate{};
        }
        QValue right = valueStack.back();
        valueStack.pop_back();
        QValue left = valueStack.back();
        valueStack.pop_back();

        // Apply operator
        QValue result = ApplyOperator(left, token.value, right);
        valueStack.push_back(result);

        std::cout << "[DEBUG] RPN eval: " << ValueToString(left) << " "
                  << token.value << " " << ValueToString(right) << " = "
                  << ValueToString(result) << std::endl;
      } else {
        // Operand - push value
        valueStack.push_back(TokenToValue(token));
      }
    }

    if (valueStack.empty()) {
      return std::monostate{};
    }

    QValue result = valueStack.back();
    std::cout << "[DEBUG] EvaluateExpression() - result: "
              << ValueToString(result) << " (" << GetValueTypeName(result)
              << ")" << std::endl;

    return result;
  }

  // Apply an operator to two values
  QValue ApplyOperator(const QValue &left, const std::string &op,
                       const QValue &right) {
    // String concatenation
    if (std::holds_alternative<std::string>(left) ||
        std::holds_alternative<std::string>(right)) {
      if (op == "+") {
        std::string leftStr = ValueToString(left);
        std::string rightStr = ValueToString(right);
        // Remove quotes from string representation
        if (leftStr.front() == '"')
          leftStr = leftStr.substr(1, leftStr.size() - 2);
        if (rightStr.front() == '"')
          rightStr = rightStr.substr(1, rightStr.size() - 2);
        return leftStr + rightStr;
      }
    }

    // Float operations
    if (std::holds_alternative<float>(left) ||
        std::holds_alternative<double>(left) ||
        std::holds_alternative<float>(right) ||
        std::holds_alternative<double>(right)) {
      double l = ToDouble(left);
      double r = ToDouble(right);

      if (op == "+")
        return static_cast<float>(l + r);
      if (op == "-")
        return static_cast<float>(l - r);
      if (op == "*")
        return static_cast<float>(l * r);
      if (op == "/")
        return r != 0.0 ? static_cast<float>(l / r) : 0.0f;
    }

    // Integer operations
    if (std::holds_alternative<int32_t>(left) ||
        std::holds_alternative<int64_t>(left)) {
      int64_t l = ToInt64(left);
      int64_t r = ToInt64(right);

      if (op == "+")
        return static_cast<int32_t>(l + r);
      if (op == "-")
        return static_cast<int32_t>(l - r);
      if (op == "*")
        return static_cast<int32_t>(l * r);
      if (op == "/")
        return r != 0 ? static_cast<int32_t>(l / r) : 0;
    }

    std::cerr << "[ERROR] ApplyOperator() - unsupported operation: "
              << GetValueTypeName(left) << " " << op << " "
              << GetValueTypeName(right) << std::endl;
    return std::monostate{};
  }

  // Convert any numeric QValue to double
  double ToDouble(const QValue &val) {
    if (std::holds_alternative<int32_t>(val))
      return static_cast<double>(std::get<int32_t>(val));
    if (std::holds_alternative<int64_t>(val))
      return static_cast<double>(std::get<int64_t>(val));
    if (std::holds_alternative<float>(val))
      return static_cast<double>(std::get<float>(val));
    if (std::holds_alternative<double>(val))
      return std::get<double>(val);
    return 0.0;
  }

  // Convert any numeric QValue to int64
  int64_t ToInt64(const QValue &val) {
    if (std::holds_alternative<int32_t>(val))
      return static_cast<int64_t>(std::get<int32_t>(val));
    if (std::holds_alternative<int64_t>(val))
      return std::get<int64_t>(val);
    if (std::holds_alternative<float>(val))
      return static_cast<int64_t>(std::get<float>(val));
    if (std::holds_alternative<double>(val))
      return static_cast<int64_t>(std::get<double>(val));
    return 0;
  }

  // Convert a token to a QValue
  QValue TokenToValue(const Token &token) {
    switch (token.type) {
    case TokenType::T_INTEGER:
      return static_cast<int32_t>(std::stoi(token.value));
    case TokenType::T_FLOAT:
      return static_cast<float>(std::stof(token.value));
    case TokenType::T_STRING:
      return token.value;
    case TokenType::T_IDENTIFIER: {
      // Check for method call (e.g., "t1.GetValue()" or "t1.ot.GetValue()")
      if (token.value.size() > 2 &&
          token.value.substr(token.value.size() - 2) == "()") {
        // This is a method call - parse instance.path.method()
        std::string withoutParens =
            token.value.substr(0, token.value.size() - 2);

        // Use rfind to find last dot - separates instance path from method name
        size_t lastDotPos = withoutParens.rfind('.');
        if (lastDotPos != std::string::npos) {
          std::string instancePath = withoutParens.substr(0, lastDotPos);
          std::string methodName = withoutParens.substr(lastDotPos + 1);

          std::cout << "[DEBUG] TokenToValue() - method call: " << instancePath
                    << "." << methodName << "()" << std::endl;

          // Split instance path by dots for chained access
          std::vector<std::string> pathParts;
          std::string current;
          for (char c : instancePath) {
            if (c == '.') {
              if (!current.empty()) {
                pathParts.push_back(current);
                current.clear();
              }
            } else {
              current += c;
            }
          }
          if (!current.empty()) {
            pathParts.push_back(current);
          }

          // Look up first instance
          std::string firstName = pathParts[0];
          QValue instanceVal = m_Context->GetVariable(firstName);
          if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(
                  instanceVal)) {
            std::cerr << "[ERROR] TokenToValue() - '" << firstName
                      << "' is not a class instance" << std::endl;
            return std::monostate{};
          }

          auto currentInstance =
              std::get<std::shared_ptr<QClassInstance>>(instanceVal);

          // Traverse remaining path parts
          for (size_t i = 1; i < pathParts.size(); i++) {
            std::string nestedName = pathParts[i];
            std::cout << "[DEBUG] TokenToValue() - traversing: " << nestedName
                      << std::endl;

            if (currentInstance->HasNestedInstance(nestedName)) {
              currentInstance = currentInstance->GetNestedInstance(nestedName);
            } else {
              std::cerr << "[ERROR] TokenToValue() - nested instance '"
                        << nestedName << "' not found" << std::endl;
              return std::monostate{};
            }
          }

          auto classDef = currentInstance->GetClassDef();

          // Find the method by name
          std::shared_ptr<QMethod> targetMethod = nullptr;
          for (const auto &method : classDef->GetMethods()) {
            if (method->GetName() == methodName) {
              targetMethod = method;
              break;
            }
          }

          if (!targetMethod) {
            std::cerr << "[ERROR] TokenToValue() - method '" << methodName
                      << "' not found" << std::endl;
            return std::monostate{};
          }

          // Execute the method
          m_HasReturn = false;
          ExecuteMethod(targetMethod, currentInstance);

          // Get the return value
          if (m_HasReturn) {
            QValue returnVal = m_ReturnValue;
            m_HasReturn = false;
            std::cout << "[DEBUG] TokenToValue() - method returned: "
                      << ValueToString(returnVal) << std::endl;
            return returnVal;
          }

          return std::monostate{}; // void method
        }
      }

      // Check for member access (e.g., "t1.num" or "t1.ot.check")
      size_t dotPos = token.value.find('.');
      if (dotPos != std::string::npos) {
        // Split full path by dots
        std::vector<std::string> parts;
        std::string current;
        for (char c : token.value) {
          if (c == '.') {
            if (!current.empty()) {
              parts.push_back(current);
              current.clear();
            }
          } else {
            current += c;
          }
        }
        if (!current.empty()) {
          parts.push_back(current);
        }

        if (parts.size() < 2) {
          std::cerr << "[ERROR] TokenToValue() - invalid member access: "
                    << token.value << std::endl;
          return std::monostate{};
        }

        std::string instanceName = parts[0];
        std::cout << "[DEBUG] TokenToValue() - chained access starting with: "
                  << instanceName << std::endl;

        // Look up initial instance
        QValue instanceVal = m_Context->GetVariable(instanceName);
        if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(
                instanceVal)) {
          std::cerr << "[ERROR] TokenToValue() - '" << instanceName
                    << "' is not a class instance" << std::endl;
          return std::monostate{};
        }

        auto currentInstance =
            std::get<std::shared_ptr<QClassInstance>>(instanceVal);

        // Traverse the chain (all but the last part)
        for (size_t i = 1; i < parts.size() - 1; i++) {
          std::string nestedName = parts[i];
          std::cout << "[DEBUG] TokenToValue() - traversing: " << nestedName
                    << std::endl;

          if (currentInstance->HasNestedInstance(nestedName)) {
            currentInstance = currentInstance->GetNestedInstance(nestedName);
          } else {
            std::cerr << "[ERROR] TokenToValue() - nested instance '"
                      << nestedName << "' not found" << std::endl;
            return std::monostate{};
          }
        }

        // The last part is the member to get
        std::string finalMember = parts.back();
        if (!currentInstance->HasMember(finalMember)) {
          std::cerr << "[ERROR] TokenToValue() - member '" << finalMember
                    << "' not found" << std::endl;
          return std::monostate{};
        }

        // Get member value and convert to QValue
        QInstanceValue instVal = currentInstance->GetMember(finalMember);
        return ConvertInstanceValueToQValue(instVal);
      }

      // Regular variable lookup
      return m_Context->GetVariable(token.value);
    }
    default:
      return token.value; // Return as string
    }
  }
};
