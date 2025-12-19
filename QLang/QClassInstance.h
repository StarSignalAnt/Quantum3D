#pragma once

#include "QClass.h"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// QValue type for instance member storage (separate from QValue in QContext)
using QInstanceValue = std::variant<std::monostate, bool, int32_t, int64_t,
                                    float, double, std::string, void *>;

// QClassInstance - represents a runtime instance of a QClass
class QClassInstance {
public:
  QClassInstance(std::shared_ptr<QClass> classDef)
      : m_ClassDef(classDef), m_ClassName(classDef->GetName()) {
    std::cout << "[DEBUG] QClassInstance created for class: " << m_ClassName
              << std::endl;
    InitializeMembers();
  }

  // Get the class name
  const std::string &GetClassName() const { return m_ClassName; }

  // Get the class definition
  std::shared_ptr<QClass> GetClassDef() const { return m_ClassDef; }

  // Set a member variable value
  void SetMember(const std::string &name, const QInstanceValue &value) {
    m_Members[name] = value;
    std::cout << "[DEBUG] QClassInstance(" << m_ClassName
              << ") - set member: " << name << std::endl;
  }

  // Get a member variable value
  QInstanceValue GetMember(const std::string &name) const {
    auto it = m_Members.find(name);
    if (it != m_Members.end()) {
      return it->second;
    }
    std::cerr << "[ERROR] QClassInstance(" << m_ClassName << ") - member '"
              << name << "' not found!" << std::endl;
    return std::monostate{};
  }

  // Check if a member exists (including nested instances)
  bool HasMember(const std::string &name) const {
    return m_Members.find(name) != m_Members.end() ||
           m_NestedInstances.find(name) != m_NestedInstances.end();
  }

  // Get all members (for debugging)
  const std::unordered_map<std::string, QInstanceValue> &GetMembers() const {
    return m_Members;
  }

  // Set a nested class instance member
  void SetNestedInstance(const std::string &name,
                         std::shared_ptr<QClassInstance> instance) {
    m_NestedInstances[name] = instance;
    std::cout << "[DEBUG] QClassInstance(" << m_ClassName
              << ") - set nested instance: " << name << std::endl;
  }

  // Get a nested class instance member
  std::shared_ptr<QClassInstance>
  GetNestedInstance(const std::string &name) const {
    auto it = m_NestedInstances.find(name);
    if (it != m_NestedInstances.end()) {
      return it->second;
    }
    return nullptr;
  }

  // Check if a nested instance exists
  bool HasNestedInstance(const std::string &name) const {
    return m_NestedInstances.find(name) != m_NestedInstances.end();
  }

  // Get all nested instance names
  std::vector<std::string> GetNestedInstanceNames() const {
    std::vector<std::string> names;
    for (const auto &[name, instance] : m_NestedInstances) {
      names.push_back(name);
    }
    return names;
  }

  // Print the instance (for debugging)
  void Print() const {
    std::cout << "Instance of " << m_ClassName << " {" << std::endl;
    for (const auto &[name, value] : m_Members) {
      std::cout << "  " << name << " = ";
      std::visit(
          [](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
              std::cout << "null";
            } else if constexpr (std::is_same_v<T, bool>) {
              std::cout << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, std::string>) {
              std::cout << "\"" << arg << "\"";
            } else if constexpr (std::is_same_v<T, void *>) {
              std::cout << "<cptr:" << arg << ">";
            } else {
              std::cout << arg;
            }
          },
          value);
      std::cout << std::endl;
    }
    std::cout << "}" << std::endl;
  }

  // Generic type mapping (T -> int32, K -> string, etc.)
  void
  SetTypeMapping(const std::unordered_map<std::string, std::string> &mapping) {
    m_TypeMapping = mapping;
    std::cout << "[DEBUG] QClassInstance(" << m_ClassName
              << ") - set type mapping" << std::endl;
  }

  const std::unordered_map<std::string, std::string> &GetTypeMapping() const {
    return m_TypeMapping;
  }

  bool HasTypeMapping() const { return !m_TypeMapping.empty(); }

private:
  std::shared_ptr<QClass> m_ClassDef;
  std::string m_ClassName;
  std::unordered_map<std::string, QInstanceValue> m_Members;
  std::unordered_map<std::string, std::shared_ptr<QClassInstance>>
      m_NestedInstances;
  std::unordered_map<std::string, std::string>
      m_TypeMapping; // Generic type mapping

  // Initialize member variables with default values from class definition
  void InitializeMembers() {
    std::cout << "[DEBUG] QClassInstance - initializing members for "
              << m_ClassName << std::endl;

    for (const auto &member : m_ClassDef->GetMembers()) {
      std::string memberName = member->GetName();
      TokenType memberType = member->GetVarType();

      // Set default value based on type
      QInstanceValue defaultVal;
      switch (memberType) {
      case TokenType::T_INT32:
        defaultVal = static_cast<int32_t>(0);
        break;
      case TokenType::T_INT64:
        defaultVal = static_cast<int64_t>(0);
        break;
      case TokenType::T_FLOAT32:
        defaultVal = 0.0f;
        break;
      case TokenType::T_FLOAT64:
        defaultVal = 0.0;
        break;
      case TokenType::T_STRING_TYPE:
        defaultVal = std::string("");
        break;
      case TokenType::T_BOOL:
        defaultVal = false;
        break;
      case TokenType::T_CPTR:
        defaultVal = static_cast<void *>(nullptr);
        break;
      default:
        defaultVal = std::monostate{};
        break;
      }

      m_Members[memberName] = defaultVal;
      std::cout << "[DEBUG] QClassInstance - initialized member: " << memberName
                << std::endl;
    }
  }
};
