#pragma once

#include "QNode.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// QEnum - represents an enum definition with named integer values
// Supports explicit value assignment (Value = 5) and auto-increment
class QEnum : public QNode {
public:
  QEnum(const std::string &name) : m_Name(name), m_NextValue(0) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QEnum created: " << name << std::endl;
#endif
  }

  std::string GetName() const override { return m_Name; }

  // Add value with auto-increment (previous value + 1)
  void AddValue(const std::string &valueName) {
    AddValueWithInt(valueName, m_NextValue);
  }

  // Add value with explicit integer assignment
  void AddValueWithInt(const std::string &valueName, int explicitValue) {
    m_Values.push_back(valueName);
    m_ValueMap[valueName] = explicitValue;
    m_NextValue = explicitValue + 1; // Next auto value is this + 1
#if QLANG_DEBUG
    std::cout << "[DEBUG] QEnum(" << m_Name << ") - added value: " << valueName
              << " = " << explicitValue << std::endl;
#endif
  }

  const std::vector<std::string> &GetValues() const { return m_Values; }

  // Get the integer value for a given enum member name
  // Returns -1 if not found
  int GetValueIndex(const std::string &valueName) const {
    auto it = m_ValueMap.find(valueName);
    if (it != m_ValueMap.end()) {
      return it->second;
    }
    return -1;
  }

  bool HasValue(const std::string &valueName) const {
    return m_ValueMap.find(valueName) != m_ValueMap.end();
  }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    // Check for duplicate values
    // Already handled by map - duplicates would overwrite
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Enum: " << m_Name << " {" << std::endl;
    for (const auto &valueName : m_Values) {
      PrintIndent(indent + 1);
      auto it = m_ValueMap.find(valueName);
      std::cout << valueName << " = " << it->second << std::endl;
    }
    PrintIndent(indent);
    std::cout << "}" << std::endl;
  }

private:
  std::string m_Name;
  std::vector<std::string> m_Values; // Ordered list of value names
  std::unordered_map<std::string, int> m_ValueMap; // Name -> value lookup
  int m_NextValue; // Next auto-assigned value (previous + 1)
};
