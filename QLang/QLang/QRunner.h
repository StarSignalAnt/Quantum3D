#pragma once

#include "Parser.h"
#include "QAssign.h"
#include "QClassInstance.h"
#include "QContext.h"
#include "QFor.h"
#include "QIncrement.h"
#include "QInstanceDecl.h"
#include "QMemberAssign.h"
#include "QMethodCall.h"
#include "QParameters.h"
#include "QReturn.h"
#include "QWhile.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <variant>

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
      if (m_HasReturn) {
        std::cout << "[DEBUG] QRunner::ExecuteCode() - return detected, "
                     "stopping execution"
                  << std::endl;
        break;
      }
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

    // Check if it's a variable assignment
    auto assign = std::dynamic_pointer_cast<QAssign>(node);
    if (assign) {
      ExecuteAssign(assign);
      return;
    }

    // Check if it's a return statement
    auto returnStmt = std::dynamic_pointer_cast<QReturn>(node);
    if (returnStmt) {
      ExecuteReturn(returnStmt);
      return;
    }

    // Check for if statement
    auto ifStmt = std::dynamic_pointer_cast<QIf>(node);
    if (ifStmt) {
      ExecuteIf(ifStmt);
      return;
    }

    // Check for for loop
    auto forStmt = std::dynamic_pointer_cast<QFor>(node);
    if (forStmt) {
      ExecuteFor(forStmt);
      return;
    }

    // Check for while loop
    auto whileStmt = std::dynamic_pointer_cast<QWhile>(node);
    if (whileStmt) {
      ExecuteWhile(whileStmt);
      return;
    }

    // Check for increment/decrement
    auto incrementStmt = std::dynamic_pointer_cast<QIncrement>(node);
    if (incrementStmt) {
      ExecuteIncrement(incrementStmt);
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

  // Find the best matching method for a given name and arguments
  std::shared_ptr<QMethod> FindMethod(std::shared_ptr<QClass> classDef,
                                      const std::string &methodName,
                                      const std::vector<QValue> &args) {
    std::cout << "[DEBUG] FindMethod() - looking for: " << methodName
              << " with " << args.size() << " args" << std::endl;

    std::shared_ptr<QMethod> bestMatch = nullptr;

    for (const auto &method : classDef->GetMethods()) {
      if (method->GetName() != methodName) {
        continue;
      }

      const auto &params = method->GetParameters();
      if (params.size() != args.size()) {
        continue;
      }

      // Check parameter types
      bool typesMatch = true;
      for (size_t i = 0; i < params.size(); i++) {
        // Basic type checking
        TokenType paramType = params[i].type;
        const auto &argValue = args[i];

        if (!CheckTypeMatch(argValue, paramType)) {
          typesMatch = false;
          break;
        }
      }

      if (typesMatch) {
        // Found a match!
        // In a more complex language, we would score matches to find the *best*
        // overload. Here, we take the first exact match we find effectively.
        // Since we check all params, this is safe enough for now.
        return method;
      }
    }

    return nullptr;
  }

  // Check if a runtime value matches a target type
  bool CheckTypeMatch(const QValue &value, TokenType type) {
    switch (type) {
    case TokenType::T_INT32:
    case TokenType::T_INT64:
    case TokenType::T_SHORT:
      return std::holds_alternative<int32_t>(value) ||
             std::holds_alternative<int64_t>(value);
    case TokenType::T_FLOAT32:
    case TokenType::T_FLOAT64:
      return std::holds_alternative<float>(value) ||
             std::holds_alternative<double>(value) ||
             std::holds_alternative<int32_t>(
                 value) || // Allow implicit int->float
             std::holds_alternative<int64_t>(value);
    case TokenType::T_STRING_TYPE:
      return std::holds_alternative<std::string>(value);
    case TokenType::T_BOOL:
      return std::holds_alternative<bool>(value);
    default:
      // For now, be lenient with other types or void
      return true;
    }
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

    // Evaluate constructor arguments
    std::vector<QValue> constructorArgs;
    auto argsParam = instanceDecl->GetConstructorArgs();
    if (argsParam) {
      for (const auto &expr : argsParam->GetParameters()) {
        QValue val = EvaluateExpression(expr);
        constructorArgs.push_back(val);
      }
    }

    // Find and execute constructor if exists
    // Constructor is a method with the same name as the class
    auto constructor = FindMethod(classDef, className, constructorArgs);
    if (constructor) {
      std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - executing "
                   "constructor for: "
                << className << std::endl;
      ExecuteMethod(constructor, instance, constructorArgs);
    } else {
      // If args were provided but no constructor found, that's an issue (unless
      // implicit default ctor)
      if (!constructorArgs.empty()) {
        std::cerr << "[ERROR] QRunner::ExecuteInstanceDecl() - no matching "
                     "constructor found for "
                  << className << " with provided arguments" << std::endl;
      } else {
        // Default constructor (no args) is optional if not defined
        std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - no default "
                     "constructor found (optional)"
                  << std::endl;
      }
    }

    // Store the instance in the context
    m_Context->SetVariable(instanceName, instance);

    std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - instance created: "
              << instanceName << std::endl;
  }

  // Execute a method call on an instance (e.g., t1.TestMeth() or
  // t1.ot.Value())
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

    // Find the method by name and args
    // We already evaluated args above in the original code?
    // Wait, the original code evaluates args AFTER finding the method.
    // We need to evaluate args BEFORE finding the method to support
    // overloading by arg types.

    // Evaluate arguments (back-to-front as per language convention - wait,
    // standard is usually left-to-right, but let's see) Original code:
    // Evaluate arguments (back-to-front as per language convention)
    // for (int i = static_cast<int>(params.size()) - 1; i >= 0; i--)

    // PROBLEM: To evaluate args, we need to know how many there are.
    // The QMethodCall has QParameters, so we know the expressions.
    // We can evaluate them all into values.

    std::vector<QValue> argValues;
    auto args = methodCall->GetArguments();
    if (args) {
      const auto &paramExprs = args->GetParameters();
      // Evaluate left-to-right is more standard, but let's stick to what was
      // there or fix it? The previous code iterated params from the *Method
      // definition*, not the call arguments? "const auto &params =
      // args->GetParameters();" -> args is QParameters, GetParameters returns
      // vector<shared_ptr<QExpression>> So it was iterating the arguments
      // provided in the call.

      // Let's evaluate them normally.
      for (const auto &expr : paramExprs) {
        argValues.push_back(EvaluateExpression(expr));
      }
    }

    std::shared_ptr<QMethod> targetMethod =
        FindMethod(classDef, methodName, argValues);

    if (!targetMethod) {
      std::cerr << "[ERROR] QRunner::ExecuteMethodCall() - method '"
                << methodName << "' not found in class '" << classDef->GetName()
                << "' matching arguments" << std::endl;
      return;
    }

    std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - found method: "
              << methodName << std::endl;
    // Arguments already evaluated above

    // Execute the method with the instance context and arguments
    ExecuteMethod(targetMethod, currentInstance, argValues);

    std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - method call complete"
              << std::endl;
  }

  // Execute a variable assignment
  void ExecuteAssign(std::shared_ptr<QAssign> assign) {
    std::string varName = assign->GetVariableName();
    std::cout << "[DEBUG] QRunner::ExecuteAssign() - assigning variable: "
              << varName << std::endl;

    QValue newValue;
    auto valExpr = assign->GetValueExpression();
    if (valExpr) {
      newValue = EvaluateExpression(valExpr);
    }

    if (m_Context->HasVariable(varName)) {
      m_Context->SetVariable(varName, newValue);
    } else {
      std::cerr << "[ERROR] QRunner::ExecuteAssign() - variable '" << varName
                << "' not declared" << std::endl;
    }
  }

  // Execute a member field assignment (e.g., t1.num = 90; or t1.ot.check =
  // 50;)
  void ExecuteMemberAssign(std::shared_ptr<QMemberAssign> memberAssign) {
    std::string instanceName = memberAssign->GetInstanceName();
    std::string memberPath = memberAssign->GetMemberName();

    std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - assigning: "
              << instanceName << "." << memberPath << std::endl;

    // Split member path by dots for chained access (e.g., "ot.check" ->
    // ["ot", "check"])
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

    // Navigate to the correct nested instance
    // The last part is the actual field name, previous parts are nested
    // instances
    for (size_t i = 0; i < pathParts.size() - 1; i++) {
      std::string nestedName = pathParts[i];
      if (currentInstance->HasNestedInstance(nestedName)) {
        currentInstance = currentInstance->GetNestedInstance(nestedName);
      } else {
        std::cerr << "[ERROR] QRunner::ExecuteMemberAssign() - nested "
                     "instance '"
                  << nestedName << "' not found" << std::endl;
        return;
      }
    }

    std::string finalMemberName = pathParts.back();

    // Evaluate the new value
    QValue newValue;
    auto valExpr = memberAssign->GetValueExpression();
    if (valExpr) {
      // Evaluate expression
      // We need to implement expression evaluation properly
      // For now, just take the first element (simple assignments)
      // TODO: Full expression evaluation
      auto elems = valExpr->GetElements();
      if (!elems.empty()) {
        newValue = EvaluateExpression(valExpr);
      }
    }

    // Update the member
    currentInstance->SetMember(finalMemberName,
                               ConvertQValueToInstanceValue(newValue));

    std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - set "
              << finalMemberName << " = " << ValueToString(newValue)
              << std::endl;

    // SYNC FIX: If we modified the current context's 'this' instance,
    // we must also update the local variable shadow to prevent it from
    // overwriting our change when the method exits (copy-back phase).
    QValue thisVal = m_Context->GetVariable("__this__");
    if (std::holds_alternative<std::shared_ptr<QClassInstance>>(thisVal)) {
      auto thisInstance = std::get<std::shared_ptr<QClassInstance>>(thisVal);
      if (currentInstance == thisInstance) {
        // We modified the current instance directly
        if (m_Context->HasLocalVariable(finalMemberName)) {
          m_Context->SetVariable(finalMemberName, newValue);
          std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - synced local "
                       "shadow: "
                    << finalMemberName << std::endl;
        }
      }
    }
  }

  // Execute an if statement
  void ExecuteIf(std::shared_ptr<QIf> ifStmt) {
    std::cout << "[DEBUG] QRunner::ExecuteIf() - executing if" << std::endl;

    // Check main condition
    QValue condVal = EvaluateExpression(ifStmt->GetCondition());
    if (IsTrue(condVal)) {
      std::cout << "[DEBUG] QRunner::ExecuteIf() - condition true, executing "
                   "then block"
                << std::endl;
      ExecuteCode(ifStmt->GetThenBlock());
      return;
    }

    // Check elseif blocks
    for (const auto &pair : ifStmt->GetElseIfBlocks()) {
      QValue elseifCond = EvaluateExpression(pair.first);
      if (IsTrue(elseifCond)) {
        std::cout << "[DEBUG] QRunner::ExecuteIf() - elseif condition true"
                  << std::endl;
        ExecuteCode(pair.second);
        return;
      }
    }

    // Execute else block if exists
    if (ifStmt->HasElse()) {
      std::cout << "[DEBUG] QRunner::ExecuteIf() - executing else block"
                << std::endl;
      ExecuteCode(ifStmt->GetElseBlock());
    }
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

    // Store 'this' reference for the method
    // We use a special variable name that TokenToValue looks for
    methodContext->SetVariable("__this__", instance);
    // Also set "this" so string lookups (like in ExecuteMemberAssign) work
    methodContext->SetVariable("this", instance);
    std::cout << "[DEBUG] QRunner::ExecuteMethod() - set 'this' reference"
              << std::endl;

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
    // Logical OR (lowest precedence)
    if (op == "||")
      return 1;
    // Logical AND
    if (op == "&&")
      return 2;
    // Comparison operators
    if (op == "==" || op == "!=")
      return 3;
    if (op == "<" || op == ">" || op == "<=" || op == ">=")
      return 4;
    // Arithmetic operators
    if (op == "+" || op == "-")
      return 5;
    if (op == "*" || op == "/")
      return 6;
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
      // Check if this starts a potential chain (identifier or 'this' followed
      // by dot)
      if ((elements[i].type == TokenType::T_IDENTIFIER ||
           elements[i].type == TokenType::T_THIS) &&
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

        // Check if this ends with ( - method call
        if (j < elements.size() && elements[j].type == TokenType::T_LPAREN) {
          // Method call with arguments
          // Consume until matching paren
          std::string fullCall = chain + "(";
          int balance = 1;
          size_t k = j + 1;

          while (k < elements.size() && balance > 0) {
            if (elements[k].type == TokenType::T_LPAREN) {
              balance++;
            } else if (elements[k].type == TokenType::T_RPAREN) {
              balance--;
            }

            if (balance > 0) {
              // Add space for separation in some cases, or just value
              // Operator/Punctuation might need spacing?
              // Ideally reconstructing source, but simple concat might work for
              // simple args
              if (elements[k].type == TokenType::T_STRING) {
                fullCall += "\"" + elements[k].value + "\"";
              } else {
                fullCall += elements[k].value;
              }
              // Add a comma separator helper? No, tokens are enough?
              // If we have "a,b", tokens are "a", ",", "b". Concat is "a,b".
              // If we have "a + b", tokens "a", "+", "b". Concat "a+b".
              // Safe enough for tokenizer re-parsing.
            }
            k++;
          }
          fullCall += ")";

          Token methodCall;
          methodCall.type = TokenType::T_IDENTIFIER;
          methodCall.value = fullCall;
          methodCall.line = elements[i].line;
          result.push_back(methodCall);
          i = k - 1; // Position after call
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

    // Preprocess unary minus: combine '-' followed by a number when it appears
    // at the start or after an operator/open-paren
    std::vector<Token> processedElements;
    for (size_t i = 0; i < elements.size(); i++) {
      const Token &token = elements[i];

      // Check if this is a unary minus
      if (token.type == TokenType::T_OPERATOR && token.value == "-") {
        bool isUnary = false;

        // Unary if at start of expression
        if (processedElements.empty()) {
          isUnary = true;
        } else {
          // Unary if previous token is an operator or open paren
          const Token &prev = processedElements.back();
          if (prev.type == TokenType::T_OPERATOR ||
              prev.type == TokenType::T_LPAREN) {
            isUnary = true;
          }
        }

        // If unary and followed by a number, combine them
        if (isUnary && i + 1 < elements.size()) {
          const Token &next = elements[i + 1];
          if (next.type == TokenType::T_INTEGER) {
            Token negativeToken;
            negativeToken.type = TokenType::T_INTEGER;
            negativeToken.value = "-" + next.value;
            negativeToken.line = token.line;
            processedElements.push_back(negativeToken);
            i++; // Skip the number we just consumed
            std::cout << "[DEBUG] EvaluateExpression() - combined unary minus: "
                      << negativeToken.value << std::endl;
            continue;
          } else if (next.type == TokenType::T_FLOAT) {
            Token negativeToken;
            negativeToken.type = TokenType::T_FLOAT;
            negativeToken.value = "-" + next.value;
            negativeToken.line = token.line;
            processedElements.push_back(negativeToken);
            i++; // Skip the number we just consumed
            std::cout << "[DEBUG] EvaluateExpression() - combined unary minus: "
                      << negativeToken.value << std::endl;
            continue;
          }
        }
      }

      processedElements.push_back(token);
    }
    elements = processedElements;

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
    // Logical operators (work on booleans)
    if (op == "&&") {
      bool l = ToBool(left);
      bool r = ToBool(right);
      return l && r;
    }
    if (op == "||") {
      bool l = ToBool(left);
      bool r = ToBool(right);
      return l || r;
    }

    // Comparison operators (return bool)
    if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" ||
        op == ">=") {
      // String comparison
      if (std::holds_alternative<std::string>(left) &&
          std::holds_alternative<std::string>(right)) {
        std::string l = std::get<std::string>(left);
        std::string r = std::get<std::string>(right);
        if (op == "==")
          return l == r;
        if (op == "!=")
          return l != r;
        if (op == "<")
          return l < r;
        if (op == ">")
          return l > r;
        if (op == "<=")
          return l <= r;
        if (op == ">=")
          return l >= r;
      }
      // Boolean comparison
      if (std::holds_alternative<bool>(left) &&
          std::holds_alternative<bool>(right)) {
        bool l = std::get<bool>(left);
        bool r = std::get<bool>(right);
        if (op == "==")
          return l == r;
        if (op == "!=")
          return l != r;
      }
      // Numeric comparison (promote to double for mixed types)
      double l = ToDouble(left);
      double r = ToDouble(right);
      if (op == "==")
        return l == r;
      if (op == "!=")
        return l != r;
      if (op == "<")
        return l < r;
      if (op == ">")
        return l > r;
      if (op == "<=")
        return l <= r;
      if (op == ">=")
        return l >= r;
    }

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

  // Helper to check if a value is true
  bool IsTrue(const QValue &val) {
    if (std::holds_alternative<bool>(val))
      return std::get<bool>(val);
    if (std::holds_alternative<int32_t>(val))
      return std::get<int32_t>(val) != 0;
    if (std::holds_alternative<int64_t>(val))
      return std::get<int64_t>(val) != 0;
    if (std::holds_alternative<float>(val))
      return std::get<float>(val) != 0.0f;
    if (std::holds_alternative<double>(val))
      return std::get<double>(val) != 0.0;
    return false;
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

  // Convert any QValue to bool (for logical operators)
  bool ToBool(const QValue &val) {
    if (std::holds_alternative<bool>(val))
      return std::get<bool>(val);
    if (std::holds_alternative<int32_t>(val))
      return std::get<int32_t>(val) != 0;
    if (std::holds_alternative<int64_t>(val))
      return std::get<int64_t>(val) != 0;
    if (std::holds_alternative<float>(val))
      return std::get<float>(val) != 0.0f;
    if (std::holds_alternative<double>(val))
      return std::get<double>(val) != 0.0;
    if (std::holds_alternative<std::string>(val))
      return !std::get<std::string>(val).empty();
    return false;
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
    case TokenType::T_TRUE:
      return true;
    case TokenType::T_FALSE:
      return false;
    case TokenType::T_THIS: {
      // 'this' refers to the current instance - look it up in context
      // The current instance should be stored with a special name
      QValue thisVal = m_Context->GetVariable("__this__");
      if (std::holds_alternative<std::shared_ptr<QClassInstance>>(thisVal)) {
        std::cout
            << "[DEBUG] TokenToValue() - resolved 'this' to current instance"
            << std::endl;
        return thisVal;
      }
      std::cerr << "[ERROR] TokenToValue() - 'this' used outside of instance "
                   "context"
                << std::endl;
      return std::monostate{};
    }
    case TokenType::T_IDENTIFIER: {
      // Check for method call (e.g., "t1.GetValue()" or "t1.ot.GetValue()" or
      // "method(1,2)")
      // We look for trailing ')'
      if (token.value.size() > 2 && token.value.back() == ')') {
        // This is a method call - parse instance.path.method(args)
        size_t openParen = token.value.find('(');
        if (openParen == std::string::npos) {
          // Should not happen if PreprocessMemberAccess works right
          return std::monostate{};
        }

        std::string pathAndMethod = token.value.substr(0, openParen);
        std::string argsStr = token.value.substr(
            openParen + 1, token.value.size() - openParen - 2);

        // Parse arguments
        std::vector<QValue> argValues;
        if (!argsStr.empty()) {
          // Use Tokenizer to parse arguments
          Tokenizer tokenizer(argsStr, true);
          tokenizer.Tokenize();
          const auto &tokens = tokenizer.GetTokens();

          // Split by comma
          std::vector<Token> currentArgTokens;
          int balance = 0;

          // Helper to execute current arg
          auto evaluateArg = [&](const std::vector<Token> &toks) {
            if (toks.empty())
              return;
            auto expr = std::make_shared<QExpression>();
            for (const auto &t : toks) {
              expr->AddElement(t);
            }
            argValues.push_back(EvaluateExpression(expr));
          };

          for (const auto &t : tokens) {
            if (t.type == TokenType::T_EOF)
              continue;

            if (t.type == TokenType::T_LPAREN)
              balance++;
            if (t.type == TokenType::T_RPAREN)
              balance--;

            if (t.type == TokenType::T_COMMA && balance == 0) {
              evaluateArg(currentArgTokens);
              currentArgTokens.clear();
            } else {
              currentArgTokens.push_back(t);
            }
          }
          evaluateArg(currentArgTokens);
        }

        // Use rfind to find last dot - separates instance path from method
        // name
        size_t lastDotPos = pathAndMethod.rfind('.');
        if (lastDotPos != std::string::npos) {
          std::string instancePath = pathAndMethod.substr(0, lastDotPos);
          std::string methodName = pathAndMethod.substr(lastDotPos + 1);

          std::cout << "[DEBUG] TokenToValue() - method call: " << instancePath
                    << "." << methodName << "() with " << argValues.size()
                    << " args" << std::endl;

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
            if (currentInstance->HasNestedInstance(nestedName)) {
              currentInstance = currentInstance->GetNestedInstance(nestedName);
            } else {
              std::cerr << "[ERROR] TokenToValue() - nested instance '"
                        << nestedName << "' not found" << std::endl;
              return std::monostate{};
            }
          }

          auto classDef = currentInstance->GetClassDef();
          auto targetMethod =
              FindMethod(classDef, methodName, argValues); // overload aware

          if (!targetMethod) {
            std::cerr << "[ERROR] TokenToValue() - method '" << methodName
                      << "' not found matching arguments" << std::endl;
            return std::monostate{};
          }

          // Execute the method
          m_HasReturn = false;
          ExecuteMethod(targetMethod, currentInstance, argValues);

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

  // Execute a for loop
  void ExecuteFor(std::shared_ptr<QFor> forStmt) {
    std::cout << "[DEBUG] QRunner::ExecuteFor() - executing for loop"
              << std::endl;

    std::string varName = forStmt->GetVarName();

    // Evaluate start, end, step
    QValue startVal = EvaluateExpression(forStmt->GetStart());
    QValue endVal = EvaluateExpression(forStmt->GetEnd());

    QValue stepVal;
    if (forStmt->GetStep()) {
      stepVal = EvaluateExpression(forStmt->GetStep());
    } else {
      stepVal = 1; // Default integer step
    }

    // If a type was declared, coerce the start value to that type
    TokenType declaredType = TokenType::T_UNKNOWN;
    if (forStmt->HasDeclaredType()) {
      declaredType = forStmt->GetVarType();
      startVal = CoerceToType(startVal, declaredType);
      std::cout << "[DEBUG] QRunner::ExecuteFor() - coerced to declared type"
                << std::endl;
    }

    // Initialize loop variable
    m_Context->SetVariable(varName, startVal);

    std::cout << "[DEBUG] QRunner::ExecuteFor() - loop initialized: " << varName
              << " = " << ValueToString(startVal) << " to "
              << ValueToString(endVal) << " step " << ValueToString(stepVal)
              << std::endl;

    // Loop
    while (true) {
      // Get current value
      QValue currentVal = m_Context->GetVariable(varName);

      // Check condition
      bool conditionMet = false;
      double currentD = GetDoubleValue(currentVal);
      double endD = GetDoubleValue(endVal);
      double stepD = GetDoubleValue(stepVal);

      if (stepD >= 0) {
        conditionMet = currentD <= endD;
      } else {
        conditionMet = currentD >= endD;
      }

      if (!conditionMet) {
        break;
      }

      // Execute body
      ExecuteCode(forStmt->GetBody());

      if (m_HasReturn) {
        break;
      }

      // Increment - use declared type if specified
      currentVal = m_Context->GetVariable(varName);
      currentD = GetDoubleValue(currentVal);

      QValue newVal;

      if (forStmt->HasDeclaredType()) {
        // Use declared type for increment
        switch (declaredType) {
        case TokenType::T_FLOAT32:
          newVal = static_cast<float>(currentD + stepD);
          break;
        case TokenType::T_FLOAT64:
          newVal = currentD + stepD;
          break;
        case TokenType::T_INT32:
          newVal = static_cast<int32_t>(currentD + stepD);
          break;
        case TokenType::T_INT64:
          newVal = static_cast<int64_t>(currentD + stepD);
          break;
        case TokenType::T_SHORT:
          newVal = static_cast<int32_t>(currentD + stepD);
          break;
        default:
          newVal = currentD + stepD;
          break;
        }
      } else {
        // Original behavior - infer from value types
        if (std::holds_alternative<int32_t>(currentVal) &&
            std::holds_alternative<int32_t>(stepVal)) {
          newVal = std::get<int32_t>(currentVal) + std::get<int32_t>(stepVal);
        } else if (std::holds_alternative<int64_t>(currentVal) &&
                   std::holds_alternative<int64_t>(stepVal)) {
          newVal = std::get<int64_t>(currentVal) + std::get<int64_t>(stepVal);
        } else {
          newVal = currentD + stepD;
        }
      }

      m_Context->SetVariable(varName, newVal);
    }

    std::cout << "[DEBUG] QRunner::ExecuteFor() - loop finished" << std::endl;
  }

  // Execute a while loop
  void ExecuteWhile(std::shared_ptr<QWhile> whileStmt) {
    std::cout << "[DEBUG] QRunner::ExecuteWhile() - executing while loop"
              << std::endl;

    int iterations = 0;
    const int maxIterations = 1000000; // Safety limit

    while (true) {
      // Evaluate condition
      QValue conditionVal = EvaluateExpression(whileStmt->GetCondition());
      bool conditionTrue = IsTrue(conditionVal);

      std::cout << "[DEBUG] QRunner::ExecuteWhile() - condition: "
                << ValueToString(conditionVal) << " = "
                << (conditionTrue ? "true" : "false") << std::endl;

      if (!conditionTrue) {
        break;
      }

      // Safety limit to prevent infinite loops
      if (++iterations > maxIterations) {
        std::cerr << "[ERROR] QRunner::ExecuteWhile() - max iterations reached"
                  << std::endl;
        break;
      }

      // Execute body
      ExecuteCode(whileStmt->GetBody());

      if (m_HasReturn) {
        std::cout << "[DEBUG] QRunner::ExecuteWhile() - return detected"
                  << std::endl;
        break;
      }
    }

    std::cout << "[DEBUG] QRunner::ExecuteWhile() - loop finished after "
              << iterations << " iterations" << std::endl;
  }

  // Execute an increment or decrement statement
  void ExecuteIncrement(std::shared_ptr<QIncrement> incrementStmt) {
    std::string varName = incrementStmt->GetVarName();
    bool isIncrement = incrementStmt->IsIncrement();

    std::cout << "[DEBUG] QRunner::ExecuteIncrement() - "
              << (isIncrement ? "incrementing" : "decrementing") << " "
              << varName << std::endl;

    // Get current value
    QValue currentVal = m_Context->GetVariable(varName);

    // Update the value based on its type
    QValue newVal;
    if (std::holds_alternative<int32_t>(currentVal)) {
      int32_t val = std::get<int32_t>(currentVal);
      newVal = isIncrement ? val + 1 : val - 1;
    } else if (std::holds_alternative<int64_t>(currentVal)) {
      int64_t val = std::get<int64_t>(currentVal);
      newVal = isIncrement ? val + 1 : val - 1;
    } else if (std::holds_alternative<float>(currentVal)) {
      float val = std::get<float>(currentVal);
      newVal = isIncrement ? val + 1.0f : val - 1.0f;
    } else if (std::holds_alternative<double>(currentVal)) {
      double val = std::get<double>(currentVal);
      newVal = isIncrement ? val + 1.0 : val - 1.0;
    } else {
      std::cerr
          << "[ERROR] QRunner::ExecuteIncrement() - cannot increment/decrement "
          << "variable '" << varName << "' of non-numeric type" << std::endl;
      return;
    }

    // Store the updated value
    m_Context->SetVariable(varName, newVal);

    std::cout << "[DEBUG] QRunner::ExecuteIncrement() - " << varName << " = "
              << ValueToString(newVal) << std::endl;
  }

  double GetDoubleValue(const QValue &val) {
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
};