#pragma once

#include "QNode.h"
#include <iostream>
#include <string>

// Represents an increment (++) or decrement (--) operation on a variable
class QIncrement : public QNode {
public:
  QIncrement(const std::string &varName, bool isIncrement)
      : m_VarName(varName), m_IsIncrement(isIncrement) {}

  std::string GetName() const override {
    return m_IsIncrement ? "Increment" : "Decrement";
  }

  const std::string &GetVarName() const { return m_VarName; }
  bool IsIncrement() const { return m_IsIncrement; }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << (m_IsIncrement ? "Increment" : "Decrement") << ": "
              << m_VarName << (m_IsIncrement ? "++" : "--") << std::endl;
  }

private:
  std::string m_VarName;
  bool m_IsIncrement; // true for ++, false for --
};
