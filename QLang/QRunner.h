#pragma once

#include "Parser.h"
#include "QAssign.h"
#include "QClassInstance.h"
#include "QContext.h"
#include "QError.h"
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
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner created" << std::endl;
#endif
  }

  QRunner(std::shared_ptr<QContext> context,
          std::shared_ptr<QErrorCollector> errorCollector)
      : m_Context(context), m_ErrorCollector(errorCollector) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner created with error collector" << std::endl;
#endif
  }

  // Error access
  std::shared_ptr<QErrorCollector> GetErrorCollector() const {
    return m_ErrorCollector;
  }
  bool HasErrors() const {
    return m_ErrorCollector && m_ErrorCollector->HasErrors();
  }
  const QCallStack &GetCallStack() const { return m_CallStack; }

  ~QRunner() {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner destroyed" << std::endl;
#endif
  }

  // Run a program
  void Run(std::shared_ptr<QProgram> program) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::Run() - starting execution" << std::endl;
#endif

    // Register class definitions from the program
    for (const auto &cls : program->GetClasses()) {
      m_Classes[cls->GetName()] = cls;
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::Run() - registered class: "
                << cls->GetName() << std::endl;
#endif
    }

    auto code = program->GetCode();
    ExecuteCode(code);

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::Run() - execution complete" << std::endl;
#endif
  }

  // ========== Introspection API ==========

  // Find a variable by name and return its value
  // Returns std::monostate{} if not found
  QValue FindVar(const std::string &name) {
    return m_Context->GetVariable(name);
  }

  // Set a variable by name
  void SetVar(const std::string &name, const QValue &value) {
    m_Context->SetVariable(name, value);
  }

  // Check if a variable exists
  bool HasVar(const std::string &name) { return m_Context->HasVariable(name); }

  // Find a class instance by variable name
  // Returns nullptr if not found or if variable is not a class instance
  std::shared_ptr<QClassInstance> FindClassInstance(const std::string &name) {
    QValue val = m_Context->GetVariable(name);
    if (std::holds_alternative<std::shared_ptr<QClassInstance>>(val)) {
      return std::get<std::shared_ptr<QClassInstance>>(val);
    }
    return nullptr;
  }

  // Get the context (for advanced use)
  std::shared_ptr<QContext> GetContext() { return m_Context; }

  // Report a runtime error
  void ReportRuntimeError(const std::string &message, int line = 0,
                          int column = 0, int length = 0) {
    if (m_ErrorCollector) {
      m_ErrorCollector->ReportRuntimeError(message, m_CallStack, line, column,
                                           length);
    } else {
      std::cerr << "[RUNTIME ERROR] " << message << std::endl;
      if (!m_CallStack.IsEmpty()) {
        std::cerr << m_CallStack.GetStackTrace() << std::endl;
      }
    }
  }

  // ========== Engine Integration API ==========

  // Find a class definition by name
  // Returns nullptr if class not found
  std::shared_ptr<QClass> FindClass(const std::string &name) {
    auto it = m_Classes.find(name);
    if (it != m_Classes.end()) {
      return it->second;
    }
    return nullptr;
  }

  // Create an instance of a class by name
  // Initializes all members (including inherited) and calls constructors
  // Returns nullptr if class not found
  std::shared_ptr<QClassInstance>
  CreateInstance(const std::string &className,
                 const std::vector<QValue> &constructorArgs = {}) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::CreateInstance() - creating: " << className
              << std::endl;
#endif

    // Find the class definition
    auto classIt = m_Classes.find(className);
    if (classIt == m_Classes.end()) {
      std::cerr << "[ERROR] QRunner::CreateInstance() - class not found: "
                << className << std::endl;
      return nullptr;
    }

    std::shared_ptr<QClass> classDef = classIt->second;

    // Create the instance
    auto instance = std::make_shared<QClassInstance>(classDef);

    // Initialize all members (including inherited members via recursion)
    std::unordered_map<std::string, std::string> typeMapping;
    InitializeInstanceMembers(instance, classDef, typeMapping);

    // Find and call constructor if exists (constructor has same name as class)
    auto constructor = FindMethodInClass(classDef, className, constructorArgs);
    if (constructor) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::CreateInstance() - calling constructor"
                << std::endl;
#endif
      ExecuteMethod(constructor, instance, constructorArgs);
    } else if (!constructorArgs.empty()) {
      ReportRuntimeError("no constructor found for class '" + className +
                         "' matching " +
                         std::to_string(constructorArgs.size()) + " arguments");
    }

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::CreateInstance() - instance created"
              << std::endl;
#endif
    return instance;
  }

  // Call a method on an instance
  // Returns the method's return value (or monostate if void)
  QValue CallMethod(std::shared_ptr<QClassInstance> instance,
                    const std::string &methodName,
                    const std::vector<QValue> &args = {}) {
    if (!instance) {
      std::cerr << "[ERROR] QRunner::CallMethod() - null instance" << std::endl;
      return std::monostate{};
    }

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::CallMethod() - calling " << methodName
              << " on " << instance->GetClassName() << std::endl;
#endif

    auto classDef = instance->GetClassDef();

    // Find the method (searches inheritance chain via FindMethod)
    auto method = FindMethod(classDef, methodName, args);
    if (!method) {
      std::cerr << "[ERROR] QRunner::CallMethod() - method '" << methodName
                << "' not found in class '" << classDef->GetName() << "'"
                << std::endl;
      return std::monostate{};
    }

    // Execute the method
    ExecuteMethod(method, instance, args);

    // Return the result if there was a return statement
    if (m_HasReturn) {
      return GetReturnValue();
    }
    return std::monostate{};
  }

private:
  std::shared_ptr<QContext> m_Context;
  std::unordered_map<std::string, std::shared_ptr<QClass>> m_Classes;
  bool m_HasReturn = false;
  QValue m_ReturnValue;

  // Error handling
  std::shared_ptr<QErrorCollector> m_ErrorCollector;
  QCallStack m_CallStack;

  // Helper methods
  void ExecuteStatement(std::shared_ptr<QStatement> stmt) {
    if (auto varDecl = std::dynamic_pointer_cast<QVariableDecl>(stmt)) {
      ExecuteVariableDecl(varDecl);
    } else if (auto assign = std::dynamic_pointer_cast<QAssign>(stmt)) {
      ExecuteAssign(assign);
    } else if (auto methodCall = std::dynamic_pointer_cast<QMethodCall>(stmt)) {
      ExecuteMethodCall(methodCall);
    } else if (auto memberAssign =
                   std::dynamic_pointer_cast<QMemberAssign>(stmt)) {
      ExecuteMemberAssign(memberAssign);
    } else if (auto ifStmt = std::dynamic_pointer_cast<QIf>(stmt)) {
      ExecuteIf(ifStmt);
    } else if (auto whileStmt = std::dynamic_pointer_cast<QWhile>(stmt)) {
      ExecuteWhile(whileStmt);
    } else if (auto forStmt = std::dynamic_pointer_cast<QFor>(stmt)) {
      ExecuteFor(forStmt);
    } else if (auto returnStmt = std::dynamic_pointer_cast<QReturn>(stmt)) {
      ExecuteReturn(returnStmt);
    } else if (auto increment = std::dynamic_pointer_cast<QIncrement>(stmt)) {
      ExecuteIncrement(increment);
    } else if (auto instanceDecl =
                   std::dynamic_pointer_cast<QInstanceDecl>(stmt)) {
      ExecuteInstanceDecl(instanceDecl);
    } else {
      // It's a base QStatement, which represents a function call in our parser
      std::string funcName = stmt->GetName();
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteStatement() - executing function: "
                << funcName << std::endl;
#endif

      // Build arguments from parameters
      std::vector<QValue> args;
      auto params = stmt->GetParameters();
      if (params) {
        for (const auto &expr : params->GetParameters()) {
          QValue value = EvaluateExpression(expr);
          args.push_back(value);
        }
      }

      // Check if this is a registered function
      if (m_Context->HasFunc(funcName)) {
        QValue result = m_Context->CallFunc(funcName, args);
#if QLANG_DEBUG
        std::cout << "[DEBUG] QRunner::ExecuteStatement() - function returned: "
                  << ValueToString(result) << std::endl;
#endif
      } else if (m_Context->HasVariable("__this__")) {
        // Resolve method on this
        QValue thisVal = m_Context->GetVariable("__this__");
        if (std::holds_alternative<std::shared_ptr<QClassInstance>>(thisVal)) {
          auto currentInstance =
              std::get<std::shared_ptr<QClassInstance>>(thisVal);
          auto classDef = currentInstance->GetClassDef();
          auto targetMethod = FindMethod(classDef, funcName, args);

          if (targetMethod) {
            ExecuteMethod(targetMethod, currentInstance, args);
            // Standalone calls ignore return value but we should clear it
            if (m_HasReturn) {
              m_HasReturn = false;
            }
          } else {
            ReportRuntimeError("unknown function or method: " + funcName);
          }
        } else {
          ReportRuntimeError("unknown function or statement: " + funcName);
        }
      } else {
        // Since we don't track statement line/col easily yet, pass 0
        ReportRuntimeError("unknown function or statement: " + funcName);
      }
    }
  }

  // Execute a QCode block
  void ExecuteCode(std::shared_ptr<QCode> code) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteCode() - executing code block"
              << std::endl;
#endif

    const auto &nodes = code->GetNodes();
    for (const auto &node : nodes) {
      ExecuteNode(node);
      if (m_HasReturn) {
#if QLANG_DEBUG
        std::cout << "[DEBUG] QRunner::ExecuteCode() - return detected, "
                     "stopping execution"
                  << std::endl;
#endif
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

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteNode() - unknown node type: "
              << node->GetName() << std::endl;
#endif
  }

  // Execute a variable declaration
  void ExecuteVariableDecl(std::shared_ptr<QVariableDecl> varDecl) {
    std::string name = varDecl->GetName();
    TokenType varType = varDecl->GetVarType();

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteVariableDecl() - declaring: " << name
              << std::endl;
#endif

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

  // Internal helper for FindMethod with strict control
  std::shared_ptr<QMethod> FindMethodInternal(
      std::shared_ptr<QClass> classDef, const std::string &methodName,
      const std::vector<QValue> &args,
      const std::unordered_map<std::string, std::string> &typeMapping = {},
      bool strict = false) {
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
        const std::string &paramTypeName = params[i].typeName;
        const auto &argValue = args[i];

        // If parameter type is identifier, check if it's a generic type
        // parameter
        if (paramType == TokenType::T_IDENTIFIER && !typeMapping.empty()) {
          auto it = typeMapping.find(paramTypeName);
          if (it != typeMapping.end()) {
            // For generic types, accept class instances
            if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(
                    argValue)) {
              typesMatch = false;
              break;
            }
            continue;
          }
        }

        // Non-generic type check
        if (!CheckTypeMatch(argValue, paramType, paramTypeName, strict)) {
          typesMatch = false;
          break;
        }
      }

      if (typesMatch) {
        return method;
      }
    }

    // Method not found in current class, check parent class (inheritance)
    if (classDef->HasParent()) {
      std::string parentName = classDef->GetParentClassName();
      auto parentIt = m_Classes.find(parentName);
      if (parentIt != m_Classes.end()) {
        return FindMethodInternal(parentIt->second, methodName, args,
                                  typeMapping, strict);
      }
    }

    return nullptr;
  }

  // Find the best matching method for a given name and arguments
  // Uses a two-pass approach: exact matches first, then fuzzy matches
  std::shared_ptr<QMethod> FindMethod(
      std::shared_ptr<QClass> classDef, const std::string &methodName,
      const std::vector<QValue> &args,
      const std::unordered_map<std::string, std::string> &typeMapping = {}) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] FindMethod() - looking for: " << methodName
              << " with " << args.size() << " args" << std::endl;
#endif

    // Pass 1: Strict match
    auto match =
        FindMethodInternal(classDef, methodName, args, typeMapping, true);
    if (match) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] FindMethod() - found exact match: " << methodName
                << std::endl;
#endif
      return match;
    }

    // Pass 2: Fuzzy match (with implicit conversions)
    match = FindMethodInternal(classDef, methodName, args, typeMapping, false);
    if (match) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] FindMethod() - found fuzzy match: " << methodName
                << std::endl;
#endif
    }
    return match;
  }

  // Internal helper for FindMethodInClass with strict control
  std::shared_ptr<QMethod>
  FindMethodInClassInternal(std::shared_ptr<QClass> classDef,
                            const std::string &methodName,
                            const std::vector<QValue> &args, bool strict) {
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
        if (!CheckTypeMatch(args[i], params[i].type, params[i].typeName,
                            strict)) {
#if QLANG_DEBUG
          std::cout << "[DEBUG] FindMethodInClassInternal() - param type "
                       "mismatch at index "
                    << i << ": expected " << params[i].typeName << " (type "
                    << static_cast<int>(params[i].type) << "), got "
                    << GetValueTypeName(args[i]) << std::endl;
#endif
          typesMatch = false;
          break;
        }
      }

      if (typesMatch) {
        return method;
      }
    }
    return nullptr;
  }

  // Find a method only in the specified class (no inheritance traversal)
  // Used for calling parent constructors specifically
  // Uses two-pass approach: exact matches first, then fuzzy matches
  std::shared_ptr<QMethod> FindMethodInClass(std::shared_ptr<QClass> classDef,
                                             const std::string &methodName,
                                             const std::vector<QValue> &args) {
    // Pass 1: Strict match
    auto match = FindMethodInClassInternal(classDef, methodName, args, true);
    if (match)
      return match;

    // Pass 2: Fuzzy match
    return FindMethodInClassInternal(classDef, methodName, args, false);
  }

  // Check if a runtime value matches a target type
  bool CheckTypeMatch(const QValue &value, TokenType type,
                      const std::string &paramTypeName = "",
                      bool strict = false) {
    if (strict) {
      switch (type) {
      case TokenType::T_INT32:
        return std::holds_alternative<int32_t>(value);
      case TokenType::T_INT64:
        return std::holds_alternative<int64_t>(value);
      case TokenType::T_SHORT:
        return std::holds_alternative<int32_t>(value);
      case TokenType::T_FLOAT32:
        return std::holds_alternative<float>(value);
      case TokenType::T_FLOAT64:
        return std::holds_alternative<double>(value);
      case TokenType::T_STRING_TYPE:
        return std::holds_alternative<std::string>(value);
      case TokenType::T_BOOL:
        return std::holds_alternative<bool>(value);
      case TokenType::T_CPTR:
        return std::holds_alternative<void *>(value);
      case TokenType::T_IDENTIFIER: {
        if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(value)) {
          return false;
        }
        if (paramTypeName.empty() || paramTypeName == "void") {
          return true;
        }
        auto instance = std::get<std::shared_ptr<QClassInstance>>(value);
        return instance->GetClassName() == paramTypeName;
      }
      default:
        return true;
      }
    }

    // Relaxed (fuzzy) matching
    switch (type) {
    case TokenType::T_INT32:
    case TokenType::T_INT64:
    case TokenType::T_SHORT:
      return std::holds_alternative<int32_t>(value) ||
             std::holds_alternative<int64_t>(value) ||
             std::holds_alternative<float>(value) ||
             std::holds_alternative<double>(value);
    case TokenType::T_FLOAT32:
    case TokenType::T_FLOAT64:
      return std::holds_alternative<float>(value) ||
             std::holds_alternative<double>(value) ||
             std::holds_alternative<int32_t>(value) ||
             std::holds_alternative<int64_t>(value);
    case TokenType::T_STRING_TYPE:
      return true; // Everything can be converted to string potentially
    case TokenType::T_BOOL:
      return true; // Everything has truthiness
    case TokenType::T_CPTR:
      return std::holds_alternative<void *>(value) ||
             std::holds_alternative<std::monostate>(value);
    case TokenType::T_IDENTIFIER: {
      // For class instances, check if types match or inherit
      if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(value)) {
        return false;
      }
      if (paramTypeName.empty() || paramTypeName == "void") {
        return true;
      }

      auto instance = std::get<std::shared_ptr<QClassInstance>>(value);
      auto instanceClass = instance->GetClassDef();

      // Check for exact match or inheritance
      auto current = instanceClass;
      while (current) {
        if (current->GetName() == paramTypeName) {
          return true;
        }
        if (!current->HasParent())
          break;
        auto parentIt = m_Classes.find(current->GetParentClassName());
        if (parentIt == m_Classes.end())
          break;
        current = parentIt->second;
      }
      return false;
    }
    default:
      return true;
    }
  }

  // Execute an instance declaration (e.g., Test t1 = new Test();)
  void ExecuteInstanceDecl(std::shared_ptr<QInstanceDecl> instanceDecl) {
    std::string className = instanceDecl->GetClassName();
    std::string instanceName = instanceDecl->GetInstanceName();

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - creating instance: "
              << className << " " << instanceName << std::endl;
#endif

    // Look up the class definition
    auto classIt = m_Classes.find(className);
    if (classIt == m_Classes.end()) {
      std::cerr << "[ERROR] QRunner::ExecuteInstanceDecl() - class not found: "
                << className << std::endl;
      return;
    }

    std::shared_ptr<QClass> classDef = classIt->second;

    // Create type mapping for generics
    std::unordered_map<std::string, std::string> typeMapping;
    if (classDef->IsGeneric() && instanceDecl->HasTypeArguments()) {
      const auto &typeParams = classDef->GetTypeParameters();
      const auto &typeArgs = instanceDecl->GetTypeArguments();

      for (size_t i = 0; i < typeParams.size() && i < typeArgs.size(); i++) {
        typeMapping[typeParams[i]] = typeArgs[i];
#if QLANG_DEBUG
        std::cout << "[DEBUG] ExecuteInstanceDecl() - type mapping: "
                  << typeParams[i] << " -> " << typeArgs[i] << std::endl;
#endif
      }
    }

    // Create a new instance of the class
    auto instance = std::make_shared<QClassInstance>(classDef);

    // Store type mapping on instance for future use
    if (!typeMapping.empty()) {
      instance->SetTypeMapping(typeMapping);
    }

    // Initialize member variables with their default expressions
    InitializeInstanceMembers(instance, classDef, typeMapping);

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
    auto constructor =
        FindMethod(classDef, className, constructorArgs, typeMapping);
    if (constructor) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - executing "
                   "constructor for: "
                << className << std::endl;
#endif
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
#if QLANG_DEBUG
        std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - no default "
                     "constructor found (optional)"
                  << std::endl;
#endif
      }
    }

    // Store the instance in the context
    m_Context->SetVariable(instanceName, instance);

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteInstanceDecl() - instance created: "
              << instanceName << std::endl;
#endif
  }

  // Execute a method call on an instance (e.g., t1.TestMeth() or
  // t1.ot.Value())
  void ExecuteMethodCall(std::shared_ptr<QMethodCall> methodCall) {
    std::string instancePath = methodCall->GetInstanceName();
    std::string methodName = methodCall->GetMethodName();

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - calling: "
              << instancePath << "." << methodName << "()" << std::endl;
#endif

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
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - traversing: "
                << nestedName << std::endl;
#endif

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

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - found method: "
              << methodName << std::endl;
#endif
    // Arguments already evaluated above

    // Execute the method with the instance context and arguments
    ExecuteMethod(targetMethod, currentInstance, argValues);

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteMethodCall() - method call complete"
              << std::endl;
#endif
  }

  // Execute a variable assignment
  void ExecuteAssign(std::shared_ptr<QAssign> assign) {
    std::string varName = assign->GetVariableName();
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteAssign() - assigning variable: "
              << varName << std::endl;
#endif

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

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - assigning: "
              << instanceName << "." << memberPath << std::endl;
#endif

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

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - set "
              << finalMemberName << " = " << ValueToString(newValue)
              << std::endl;
#endif

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
#if QLANG_DEBUG
          std::cout << "[DEBUG] QRunner::ExecuteMemberAssign() - synced local "
                       "shadow: "
                    << finalMemberName << std::endl;
#endif
        }
      }
    }
  }

  // Execute an if statement
  void ExecuteIf(std::shared_ptr<QIf> ifStmt) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteIf() - executing if" << std::endl;
#endif

    // Check main condition
    QValue condVal = EvaluateExpression(ifStmt->GetCondition());
    if (IsTrue(condVal)) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteIf() - condition true, executing "
                   "then block"
                << std::endl;
#endif
      ExecuteCode(ifStmt->GetThenBlock());
      return;
    }

    // Check elseif blocks
    for (const auto &pair : ifStmt->GetElseIfBlocks()) {
      QValue elseifCond = EvaluateExpression(pair.first);
      if (IsTrue(elseifCond)) {
#if QLANG_DEBUG
        std::cout << "[DEBUG] QRunner::ExecuteIf() - elseif condition true"
                  << std::endl;
#endif
        ExecuteCode(pair.second);
        return;
      }
    }

    // Execute else block if exists
    if (ifStmt->HasElse()) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteIf() - executing else block"
                << std::endl;
#endif
      ExecuteCode(ifStmt->GetElseBlock());
    }
  }

  // Execute a return statement
  void ExecuteReturn(std::shared_ptr<QReturn> returnStmt) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteReturn() - executing return"
              << std::endl;
#endif

    if (returnStmt->HasExpression()) {
      m_ReturnValue = EvaluateExpression(returnStmt->GetExpression());
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteReturn() - return value: "
                << ValueToString(m_ReturnValue) << std::endl;
#endif
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
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteMethod() - executing method: "
              << method->GetName() << std::endl;
#endif

    // Create a child context for the method execution
    // This context will have access to instance members as local variables
    auto methodContext =
        std::make_shared<QContext>("method:" + method->GetName(), m_Context);

    // Copy instance member variables into the method's local context
    for (const auto &[memberName, memberValue] : instance->GetMembers()) {
      // Convert QInstanceValue to QValue
      QValue qval = ConvertInstanceValueToQValue(memberValue);
      methodContext->SetVariable(memberName, qval);
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteMethod() - loaded member: "
                << memberName << std::endl;
#endif
    }

    // Also load nested class instances into the method context
    // This allows access to nested instance members like ot.check
    for (const auto &nestedName : instance->GetNestedInstanceNames()) {
      auto nestedInstance = instance->GetNestedInstance(nestedName);
      if (nestedInstance) {
        methodContext->SetVariable(nestedName, nestedInstance);
#if QLANG_DEBUG
        std::cout
            << "[DEBUG] QRunner::ExecuteMethod() - loaded nested instance: "
            << nestedName << std::endl;
#endif
      }
    }

    // Store 'this' reference for the method
    // We use a special variable name that TokenToValue looks for
    methodContext->SetVariable("__this__", instance);
    // Also set "this" so string lookups (like in ExecuteMemberAssign) work
    methodContext->SetVariable("this", instance);
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteMethod() - set 'this' reference"
              << std::endl;
#endif

    // Push to call stack
    m_CallStack.Push(method->GetName(), instance->GetClassName());

    // Bind arguments to parameters (with coercion)
    const auto &params = method->GetParameters();
    for (size_t i = 0; i < params.size() && i < args.size(); i++) {
      QValue coercedArg = CoerceToType(args[i], params[i].type);
      methodContext->SetVariable(params[i].name, coercedArg);
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteMethod() - bound param "
                << params[i].name << " = " << ValueToString(coercedArg)
                << " (from " << ValueToString(args[i]) << ")" << std::endl;
#endif
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
#if QLANG_DEBUG
        std::cout << "[DEBUG] QRunner::ExecuteMethod() - updated member: "
                  << memberName << std::endl;
#endif
      }
    }

    // Restore original context
    m_Context = savedContext;

    // Pop from call stack
    m_CallStack.Pop();

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteMethod() - method complete: "
              << method->GetName() << std::endl;
#endif
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
    if (std::holds_alternative<void *>(instVal))
      return std::get<void *>(instVal);
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
    if (std::holds_alternative<void *>(qval))
      return std::get<void *>(qval);
    // Class instances stay as they are in the parent scope
    return std::monostate{};
  }

  // Initialize instance members with their default expressions from the class
  void InitializeInstanceMembers(
      std::shared_ptr<QClassInstance> instance,
      std::shared_ptr<QClass> classDef,
      const std::unordered_map<std::string, std::string> &typeMapping = {}) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::InitializeInstanceMembers() - initializing "
                 "members for: "
              << classDef->GetName() << std::endl;
#endif

    // First, initialize parent class members (inheritance)
    if (classDef->HasParent()) {
      std::string parentName = classDef->GetParentClassName();
      auto parentIt = m_Classes.find(parentName);
      if (parentIt != m_Classes.end()) {
#if QLANG_DEBUG
        std::cout << "[DEBUG] InitializeInstanceMembers() - initializing "
                     "parent members from: "
                  << parentName << std::endl;
#endif
        InitializeInstanceMembers(instance, parentIt->second, typeMapping);

        // Call parent's default constructor if it exists
        auto parentClassDef = parentIt->second;
        std::vector<QValue> emptyArgs;
        auto parentConstructor =
            FindMethodInClass(parentClassDef, parentName, emptyArgs);
        if (parentConstructor) {
#if QLANG_DEBUG
          std::cout << "[DEBUG] InitializeInstanceMembers() - calling parent "
                       "constructor: "
                    << parentName << std::endl;
#endif
          ExecuteMethod(parentConstructor, instance, emptyArgs);
        } else {
          std::cout << "[DEBUG] InitializeInstanceMembers() - no parent "
                       "constructor found (optional)"
                    << std::endl;
        }
      } else {
        std::cerr << "[ERROR] InitializeInstanceMembers() - parent class not "
                     "found: "
                  << parentName << std::endl;
      }
    }

    for (const auto &member : classDef->GetMembers()) {
      std::string memberName = member->GetName();
      TokenType memberType = member->GetVarType();
      std::string typeName = member->GetTypeName();

      // Check if this is a generic type parameter that needs resolution
      if (memberType == TokenType::T_IDENTIFIER && !typeMapping.empty()) {
        auto it = typeMapping.find(typeName);
        if (it != typeMapping.end()) {
          // Resolve the generic type to concrete type
          std::string concreteType = it->second;
          TokenType resolvedType = TypeNameToTokenType(concreteType);
#if QLANG_DEBUG
          std::cout
              << "[DEBUG] InitializeInstanceMembers() - resolved generic type "
              << typeName << " -> " << concreteType << std::endl;
#endif
          memberType = resolvedType;
        }
      }

      QValue value;

      // Check if this is a class instance member (non-primitive type)
      // T_IDENTIFIER means it's a class type (after resolution, if still
      // identifier)
      if (memberType == TokenType::T_IDENTIFIER && member->HasInitializer()) {
        const auto &initExpr = member->GetInitializer();
        const auto &elements = initExpr->GetElements();

        // Check for "new ClassName()" pattern
        // Pattern: [new] [ClassName] [(] [)] or [new] [ClassName] [(] args...
        // [)]
        if (elements.size() >= 3 && elements[0].type == TokenType::T_NEW &&
            elements[1].type == TokenType::T_IDENTIFIER) {
          std::string nestedClassName = elements[1].value;

#if QLANG_DEBUG
          std::cout << "[DEBUG] QRunner::InitializeInstanceMembers() - "
                       "creating nested instance: "
                    << nestedClassName << std::endl;
#endif

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
#if QLANG_DEBUG
              std::cout << "[DEBUG] InitializeInstanceMembers() - executing "
                           "nested constructor: "
                        << nestedClassName << std::endl;
#endif
              ExecuteMethod(method, nestedInstance);
              break;
            }
          }

          // Store nested instance as this member's value
          // Note: We store as QValue (shared_ptr<QClassInstance>) directly
          value = nestedInstance;

#if QLANG_DEBUG
          std::cout << "[DEBUG] InitializeInstanceMembers() - nested instance "
                       "created: "
                    << memberName << std::endl;
#endif
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
#if QLANG_DEBUG
        std::cout << "[DEBUG] InitializeInstanceMembers() - storing nested "
                     "instance reference for: "
                  << memberName << std::endl;
#endif
        // Note: QInstanceValue can't hold shared_ptr<QClassInstance>
        // We'll store it in a separate map in QClassInstance
        instance->SetNestedInstance(
            memberName, std::get<std::shared_ptr<QClassInstance>>(value));
      } else {
        // Convert QValue to QInstanceValue and set on instance
        QInstanceValue instVal = ConvertQValueToInstanceValue(value);
        instance->SetMember(memberName, instVal);

#if QLANG_DEBUG
        std::cout << "[DEBUG] QRunner::InitializeInstanceMembers() - set "
                  << memberName << " = " << ValueToString(value) << std::endl;
#endif
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
    case TokenType::T_CPTR:
      return static_cast<void *>(nullptr);
    default:
      return std::monostate{};
    }
  }

  // Convert type name string to TokenType (for generic type resolution)
  TokenType TypeNameToTokenType(const std::string &typeName) {
    if (typeName == "int32")
      return TokenType::T_INT32;
    if (typeName == "int64")
      return TokenType::T_INT64;
    if (typeName == "float32")
      return TokenType::T_FLOAT32;
    if (typeName == "float64")
      return TokenType::T_FLOAT64;
    if (typeName == "short")
      return TokenType::T_SHORT;
    if (typeName == "string")
      return TokenType::T_STRING_TYPE;
    if (typeName == "bool")
      return TokenType::T_BOOL;
    if (typeName == "cptr")
      return TokenType::T_CPTR;
    return TokenType::T_IDENTIFIER; // Unknown type, treat as class
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
      // Check for 'new' keyword - skip it and combine the following
      // call/identifier
      bool hasNew = false;
      if (elements[i].type == TokenType::T_NEW) {
        hasNew = true;
        i++;
        if (i >= elements.size()) {
          result.push_back(
              elements[i - 1]); // Should be an error but avoid crash
          break;
        }
      }

      // Check if this starts a potential chain (identifier or 'this' followed
      // by dot OR identifier followed by '(' for standalone call)
      if ((elements[i].type == TokenType::T_IDENTIFIER ||
           elements[i].type == TokenType::T_THIS) &&
          i + 1 < elements.size() &&
          (elements[i + 1].type == TokenType::T_DOT ||
           elements[i + 1].type == TokenType::T_LPAREN)) {

        bool isStandaloneCall = (elements[i + 1].type == TokenType::T_LPAREN);
        std::string chain = elements[i].value;
        size_t j = i + 1;

        if (!isStandaloneCall) {
          // Build the full chain by consuming .identifier patterns
          while (j + 1 < elements.size() &&
                 elements[j].type == TokenType::T_DOT &&
                 elements[j + 1].type == TokenType::T_IDENTIFIER) {
            chain += "." + elements[j + 1].value;
            j += 2; // Skip dot and identifier
          }
        }

        // Check if this is/ends with ( - method call
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
              if (elements[k].type == TokenType::T_STRING) {
                fullCall += "\"" + elements[k].value + "\"";
              } else {
                fullCall += elements[k].value;
              }
            }
            k++;
          }
          fullCall += ")";

          Token methodCall;
          methodCall.type = hasNew ? TokenType::T_NEW : TokenType::T_IDENTIFIER;
          methodCall.value = fullCall;
          methodCall.line = elements[i].line;
          result.push_back(methodCall);
          i = k - 1; // Position after call
#if QLANG_DEBUG
          std::cout << "[DEBUG] PreprocessMemberAccess() - "
                    << (hasNew ? "new " : "")
                    << "method call: " << methodCall.value << std::endl;
#endif
        } else {
          // Member access
          Token memberAccess;
          memberAccess.type =
              hasNew ? TokenType::T_NEW : TokenType::T_IDENTIFIER;
          memberAccess.value = chain;
          memberAccess.line = elements[i].line;
          result.push_back(memberAccess);
          i = j - 1; // Position at last consumed token
#if QLANG_DEBUG
          std::cout << "[DEBUG] PreprocessMemberAccess() - "
                    << (hasNew ? "new " : "")
                    << "combined: " << memberAccess.value << std::endl;
#endif
        }
      } else {
        if (hasNew) {
          // new followed by just identifier (no call)
          Token newIdent;
          newIdent.type = TokenType::T_NEW;
          newIdent.value = elements[i].value;
          newIdent.line = elements[i].line;
          result.push_back(newIdent);
        } else {
          result.push_back(elements[i]);
        }
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
#if QLANG_DEBUG
            std::cout << "[DEBUG] EvaluateExpression() - combined unary minus: "
                      << negativeToken.value << std::endl;
#endif
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

#if QLANG_DEBUG
    std::cout << "[DEBUG] EvaluateExpression() - using Shunting Yard RPN"
              << std::endl;
#endif

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
#if QLANG_DEBUG
    std::cout << "[DEBUG] RPN: ";
    for (const auto &t : outputQueue) {
      std::cout << t.value << " ";
    }
    std::cout << std::endl;
#endif

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

#if QLANG_DEBUG
        std::cout << "[DEBUG] RPN eval: " << ValueToString(left) << " "
                  << token.value << " " << ValueToString(right) << " = "
                  << ValueToString(result) << std::endl;
#endif
      } else {
        // Operand - push value
        valueStack.push_back(TokenToValue(token));
      }
    }

    if (valueStack.empty()) {
      return std::monostate{};
    }

    QValue result = valueStack.back();
#if QLANG_DEBUG
    std::cout << "[DEBUG] EvaluateExpression() - result: "
              << ValueToString(result) << " (" << GetValueTypeName(result)
              << ")" << std::endl;
#endif

    return result;
  }

  // Apply an operator to two values
  QValue ApplyOperator(const QValue &left, const std::string &op,
                       const QValue &right) {
    // Check for operator overloading on class instances
    if (std::holds_alternative<std::shared_ptr<QClassInstance>>(left)) {
      auto instance = std::get<std::shared_ptr<QClassInstance>>(left);
      std::string methodName;
      if (op == "+")
        methodName = "Plus";
      else if (op == "-")
        methodName = "Minus";
      else if (op == "*")
        methodName = "Multiply";
      else if (op == "/")
        methodName = "Divide";

      if (!methodName.empty()) {
        // Check if method exists before calling to avoid error spam if not
        // overloaded
        if (FindMethod(instance->GetClassDef(), methodName, {right})) {
          return CallMethod(instance, methodName, {right});
        }
      }
    }

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
      // Null comparison - check if either side is null (monostate)
      bool leftIsNull = std::holds_alternative<std::monostate>(left);
      bool rightIsNull = std::holds_alternative<std::monostate>(right);

      if (leftIsNull || rightIsNull) {
        // null == null is true, null != null is false
        // value == null is true only if value is also null
        if (op == "==") {
          return leftIsNull && rightIsNull;
        }
        if (op == "!=") {
          return leftIsNull != rightIsNull; // true if only one is null
        }
        // Other comparisons with null are always false
        return false;
      }

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
  // Check if a value is truthy (delegates to ToBool for consistent truthiness)
  bool IsTrue(const QValue &val) { return ToBool(val); }

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

  // Convert any QValue to bool (for truthiness and logical operators)
  bool ToBool(const QValue &val) {
    // null is falsy
    if (std::holds_alternative<std::monostate>(val))
      return false;
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
    // Class instances are truthy
    if (std::holds_alternative<std::shared_ptr<QClassInstance>>(val))
      return true;
    // void* pointers are truthy if non-null
    if (std::holds_alternative<void *>(val))
      return std::get<void *>(val) != nullptr;
    return false;
  }

  // Convert a token to a QValue
  QValue TokenToValue(const Token &token) {
    switch (token.type) {
    case TokenType::T_NEW:
    case TokenType::T_IDENTIFIER: {
      bool isNew = (token.type == TokenType::T_NEW);
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

        if (isNew) {
          // Case: new ClassName(args)
          return CreateInstance(pathAndMethod, argValues);
        }

        // Use rfind to find last dot - separates instance path from method
        // name
        size_t lastDotPos = pathAndMethod.rfind('.');
        if (lastDotPos != std::string::npos) {
          std::string instancePath = pathAndMethod.substr(0, lastDotPos);
          std::string methodName = pathAndMethod.substr(lastDotPos + 1);

#if QLANG_DEBUG
          std::cout << "[DEBUG] TokenToValue() - method call: " << instancePath
                    << "." << methodName << "() with " << argValues.size()
                    << " args" << std::endl;
#endif

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

          if (!m_Context->HasVariable(firstName)) {
            // Note: We don't have token for firstName here easily unless we
            // track it
            ReportRuntimeError("unknown variable '" + token.value + "'");
            return std::monostate{};
          }

          QValue instanceVal = m_Context->GetVariable(firstName);
          if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(
                  instanceVal)) {
            ReportRuntimeError("'" + firstName + "' is not a class instance");
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
#if QLANG_DEBUG
            std::cout << "[DEBUG] TokenToValue() - method returned: "
                      << ValueToString(returnVal) << std::endl;
#endif
            return returnVal;
          }

          return std::monostate{}; // void method
        } else {
          // Standalone call (no dot) - e.g. "GetPosition()"
          std::string methodName = pathAndMethod;

#if QLANG_DEBUG
          std::cout << "[DEBUG] TokenToValue() - standalone call: "
                    << methodName << "() with " << argValues.size() << " args"
                    << std::endl;
#endif

          // 1. Check for global native function
          if (m_Context->HasFunc(methodName)) {
            return m_Context->CallFunc(methodName, argValues);
          }

          // 2. Check if we are in an instance context (__this__)
          if (m_Context->HasVariable("__this__")) {
            QValue thisVal = m_Context->GetVariable("__this__");
            if (std::holds_alternative<std::shared_ptr<QClassInstance>>(
                    thisVal)) {
              auto currentInstance =
                  std::get<std::shared_ptr<QClassInstance>>(thisVal);
              auto classDef = currentInstance->GetClassDef();
              auto targetMethod = FindMethod(classDef, methodName, argValues);

              if (targetMethod) {
                // Execute the method
                m_HasReturn = false;
                ExecuteMethod(targetMethod, currentInstance, argValues);

                // Get the return value
                if (m_HasReturn) {
                  QValue returnVal = m_ReturnValue;
                  m_HasReturn = false;
#if QLANG_DEBUG
                  std::cout
                      << "[DEBUG] TokenToValue() - standalone method returned: "
                      << ValueToString(returnVal) << std::endl;
#endif
                  return returnVal;
                }
                return std::monostate{}; // void method
              }
            }
          }

          // 3. Check if it's a constructor call (e.g. Vec3(1,2,3) without
          // 'new')
          if (m_Classes.find(methodName) != m_Classes.end()) {
#if QLANG_DEBUG
            std::cout << "[DEBUG] TokenToValue() - resolved as implicit "
                         "constructor for class: "
                      << methodName << std::endl;
#endif
            return CreateInstance(methodName, argValues);
          }

          ReportRuntimeError("unknown function or method '" + methodName + "'");
          return std::monostate{};
        }
      }

      if (isNew) {
        // new ClassName without parens? (e.g. new Vec3)
        // We support this as empty arg constructor call
        if (m_Classes.find(token.value) != m_Classes.end()) {
          return CreateInstance(token.value, {});
        }
        ReportRuntimeError("unknown class for 'new': " + token.value);
        return std::monostate{};
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
#if QLANG_DEBUG
        std::cout << "[DEBUG] TokenToValue() - chained access starting with: "
                  << instanceName << std::endl;
#endif

        // Look up initial instance
        // Look up initial instance
        if (!m_Context->HasVariable(instanceName)) {
          ReportRuntimeError("unknown variable '" + instanceName + "'",
                             token.line, token.column,
                             static_cast<int>(token.value.length()));
          return std::monostate{};
        }

        QValue instanceVal = m_Context->GetVariable(instanceName);
        if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(
                instanceVal)) {
          ReportRuntimeError("'" + instanceName + "' is not a class instance",
                             token.line, token.column,
                             static_cast<int>(token.value.length()));
          return std::monostate{};
        }

        auto currentInstance =
            std::get<std::shared_ptr<QClassInstance>>(instanceVal);

        // Traverse the chain (all but the last part)
        for (size_t i = 1; i < parts.size() - 1; i++) {
          std::string nestedName = parts[i];
#if QLANG_DEBUG
          std::cout << "[DEBUG] TokenToValue() - traversing: " << nestedName
                    << std::endl;
#endif

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
      if (m_Context->HasVariable(token.value)) {
        return m_Context->GetVariable(token.value);
      } else {
        ReportRuntimeError("unknown variable '" + token.value + "'", token.line,
                           token.column,
                           static_cast<int>(token.value.length()));
        return std::monostate{};
      }
    }

    case TokenType::T_INTEGER:
      try {
        if (token.value.find("0x") == 0 || token.value.find("0X") == 0) {
          return static_cast<int32_t>(std::stoll(token.value, nullptr, 16));
        }
        return std::stoi(token.value);
      } catch (...) {
        try {
          return static_cast<int64_t>(std::stoll(token.value));
        } catch (...) {
          return 0;
        }
      }
    case TokenType::T_FLOAT:
      try {
        return std::stof(token.value);
      } catch (...) {
        return 0.0f;
      }
    case TokenType::T_STRING:
      return token.value;
    case TokenType::T_TRUE:
      return true;
    case TokenType::T_FALSE:
      return false;
    case TokenType::T_NULL:
      return std::monostate{};
    default:
      return token.value; // Return as string
    }
  }

  // Execute a for loop
  void ExecuteFor(std::shared_ptr<QFor> forStmt) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteFor() - executing for loop"
              << std::endl;
#endif

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
#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteFor() - coerced to declared type"
                << std::endl;
#endif
    }

    // Initialize loop variable
    m_Context->SetVariable(varName, startVal);

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteFor() - loop initialized: " << varName
              << " = " << ValueToString(startVal) << " to "
              << ValueToString(endVal) << " step " << ValueToString(stepVal)
              << std::endl;
#endif

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

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteFor() - loop finished" << std::endl;
#endif
  }

  // Execute a while loop
  void ExecuteWhile(std::shared_ptr<QWhile> whileStmt) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteWhile() - executing while loop"
              << std::endl;
#endif

    int iterations = 0;
    const int maxIterations = 1000000; // Safety limit

    while (true) {
      // Evaluate condition
      QValue conditionVal = EvaluateExpression(whileStmt->GetCondition());
      bool conditionTrue = IsTrue(conditionVal);

#if QLANG_DEBUG
      std::cout << "[DEBUG] QRunner::ExecuteWhile() - condition: "
                << ValueToString(conditionVal) << " = "
                << (conditionTrue ? "true" : "false") << std::endl;
#endif

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
#if QLANG_DEBUG
        std::cout << "[DEBUG] QRunner::ExecuteWhile() - return detected"
                  << std::endl;
#endif
        break;
      }
    }

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteWhile() - loop finished after "
              << iterations << " iterations" << std::endl;
#endif
  }

  // Execute an increment or decrement statement
  void ExecuteIncrement(std::shared_ptr<QIncrement> incrementStmt) {
    std::string varName = incrementStmt->GetVarName();
    bool isIncrement = incrementStmt->IsIncrement();

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteIncrement() - "
              << (isIncrement ? "incrementing" : "decrementing") << " "
              << varName << std::endl;
#endif

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

#if QLANG_DEBUG
    std::cout << "[DEBUG] QRunner::ExecuteIncrement() - " << varName << " = "
              << ValueToString(newVal) << std::endl;
#endif
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