#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// Forward declarations
class QContext;
class QClassInstance;

// QValue - represents a value in the context (can be various types)
using QValue = std::variant<std::monostate,                 // null/undefined
                            bool,                           // bool
                            int32_t,                        // int32
                            int64_t,                        // int64
                            float,                          // float32
                            double,                         // float64
                            std::string,                    // string
                            void *,                         // cptr (C pointer)
                            std::shared_ptr<QClassInstance> // class instance
                            >;

// Native function signature: takes context and vector of arguments, returns a
// value
using QNativeFunc =
    std::function<QValue(QContext *, const std::vector<QValue> &)>;

// Helper to get type name for debugging
inline std::string GetValueTypeName(const QValue &value) {
  if (std::holds_alternative<std::monostate>(value))
    return "null";
  if (std::holds_alternative<bool>(value))
    return "bool";
  if (std::holds_alternative<int32_t>(value))
    return "int32";
  if (std::holds_alternative<int64_t>(value))
    return "int64";
  if (std::holds_alternative<float>(value))
    return "float32";
  if (std::holds_alternative<double>(value))
    return "float64";
  if (std::holds_alternative<std::string>(value))
    return "string";
  if (std::holds_alternative<void *>(value))
    return "cptr";
  if (std::holds_alternative<std::shared_ptr<QClassInstance>>(value))
    return "instance";
  return "unknown";
}

// Helper to convert value to string for debugging
inline std::string ValueToString(const QValue &value) {
  if (std::holds_alternative<std::monostate>(value))
    return "null";
  if (std::holds_alternative<bool>(value))
    return std::get<bool>(value) ? "true" : "false";
  if (std::holds_alternative<int32_t>(value))
    return std::to_string(std::get<int32_t>(value));
  if (std::holds_alternative<int64_t>(value))
    return std::to_string(std::get<int64_t>(value));
  if (std::holds_alternative<float>(value))
    return std::to_string(std::get<float>(value));
  if (std::holds_alternative<double>(value))
    return std::to_string(std::get<double>(value));
  if (std::holds_alternative<std::string>(value))
    return "\"" + std::get<std::string>(value) + "\"";
  if (std::holds_alternative<void *>(value)) {
    std::ostringstream oss;
    oss << "<cptr:" << std::get<void *>(value) << ">";
    return oss.str();
  }
  if (std::holds_alternative<std::shared_ptr<QClassInstance>>(value)) {
    return "<instance>";
  }
  return "unknown";
}

// QContext - manages variable scopes for program execution
// Supports nested contexts for method calls (local scope -> member scope ->
// parent scope)
class QContext {
public:
  // Create a root context (no parent)
  QContext(const std::string &name = "root") : m_Name(name), m_Parent(nullptr) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QContext created: " << name << std::endl;
#endif
  }

  // Create a child context with a parent
  QContext(const std::string &name, std::shared_ptr<QContext> parent)
      : m_Name(name), m_Parent(parent) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QContext created: " << name
              << " (parent: " << parent->GetName() << ")" << std::endl;
#endif
  }

  ~QContext() {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QContext destroyed: " << m_Name << std::endl;
#endif
  }

  const std::string &GetName() const { return m_Name; }

  // Set a variable in this context
  void SetVariable(const std::string &name, const QValue &value) {
    m_Variables[name] = value;
#if QLANG_DEBUG
    std::cout << "[DEBUG] QContext(" << m_Name << ") - set variable: " << name
              << " = " << ValueToString(value) << " ("
              << GetValueTypeName(value) << ")" << std::endl;
#endif
  }

  // Get a variable - searches this context first, then parent contexts
  QValue GetVariable(const std::string &name) const {
    auto it = m_Variables.find(name);
    if (it != m_Variables.end()) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] QContext(" << m_Name
                << ") - found variable: " << name << " = "
                << ValueToString(it->second) << std::endl;
#endif
      return it->second;
    }

    // Search parent context
    if (m_Parent) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] QContext(" << m_Name << ") - variable '" << name
                << "' not found, searching parent..." << std::endl;
#endif
      return m_Parent->GetVariable(name);
    }

#if QLANG_DEBUG
    std::cout << "[DEBUG] QContext(" << m_Name << ") - variable '" << name
              << "' not found!" << std::endl;
#endif
    return std::monostate{}; // Return null if not found
  }

  // Check if a variable exists in this context (not parents)
  bool HasLocalVariable(const std::string &name) const {
    return m_Variables.find(name) != m_Variables.end();
  }

  // Check if a variable exists in this context or any parent
  bool HasVariable(const std::string &name) const {
    if (HasLocalVariable(name))
      return true;
    if (m_Parent)
      return m_Parent->HasVariable(name);
    return false;
  }

  // ========== Native Function Support ==========

  // Add a native C++ function to the context
  void AddFunc(const std::string &name, QNativeFunc func) {
    m_Functions[name] = func;
#if QLANG_DEBUG
    std::cout << "[DEBUG] QContext(" << m_Name << ") - added function: " << name
              << std::endl;
#endif
  }

  // Check if a function exists
  bool HasFunc(const std::string &name) const {
    if (m_Functions.find(name) != m_Functions.end())
      return true;
    if (m_Parent)
      return m_Parent->HasFunc(name);
    return false;
  }

  // Call a native function by name
  QValue CallFunc(const std::string &name, const std::vector<QValue> &args) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QContext(" << m_Name
              << ") - calling function: " << name << " with " << args.size()
              << " args" << std::endl;
#endif

    // Search in this context
    auto it = m_Functions.find(name);
    if (it != m_Functions.end()) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] QContext(" << m_Name
                << ") - found function: " << name << std::endl;
#endif
      return it->second(this, args);
    }

    // Search parent context
    if (m_Parent) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] QContext(" << m_Name << ") - function '" << name
                << "' not found, searching parent..." << std::endl;
#endif
      return m_Parent->CallFunc(name, args);
    }

    std::cerr << "[ERROR] QContext(" << m_Name << ") - function '" << name
              << "' not found!" << std::endl;
    return std::monostate{};
  }

  // Create a child context (for method calls, blocks, etc.)
  std::shared_ptr<QContext> CreateChildContext(const std::string &name) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QContext(" << m_Name
              << ") - creating child context: " << name << std::endl;
#endif
    // Note: We need to use shared_from_this pattern properly
    // For now, we'll create a new context with this as parent
    return std::make_shared<QContext>(
        name, std::shared_ptr<QContext>(this, [](QContext *) {}));
  }

  // Get the parent context
  std::shared_ptr<QContext> GetParent() const { return m_Parent; }

  // Print all variables in this context (for debugging)
  void PrintVariables(int indent = 0) const {
    std::string indentStr(indent * 2, ' ');
    std::cout << indentStr << "Context: " << m_Name << " {" << std::endl;
    for (const auto &[name, value] : m_Variables) {
      std::cout << indentStr << "  " << name << " = " << ValueToString(value)
                << " (" << GetValueTypeName(value) << ")" << std::endl;
    }
    std::cout << indentStr << "  Functions: ";
    for (const auto &[name, func] : m_Functions) {
      std::cout << name << " ";
    }
    std::cout << std::endl;
    std::cout << indentStr << "}" << std::endl;

    if (m_Parent) {
      std::cout << indentStr << "Parent:" << std::endl;
      m_Parent->PrintVariables(indent + 1);
    }
  }

private:
  std::string m_Name;
  std::shared_ptr<QContext> m_Parent;
  std::unordered_map<std::string, QValue> m_Variables;
  std::unordered_map<std::string, QNativeFunc> m_Functions;
};
