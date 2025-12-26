#pragma once

#include "QExpression.h"
#include "QNode.h"
#include <iostream>
#include <memory>
#include <string>

// QAssign - represents a variable assignment (e.g., val = 5; or ptr[idx] = 5;)
class QAssign : public QNode {
public:
  QAssign(const std::string &variableName) : m_VariableName(variableName) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QAssign created: " << variableName << std::endl;
#endif
  }

  std::string GetName() const override { return "Assign"; }
  std::string GetVariableName() const { return m_VariableName; }

  void SetValueExpression(std::shared_ptr<QExpression> expr) {
    m_ValueExpression = expr;
  }

  std::shared_ptr<QExpression> GetValueExpression() const {
    return m_ValueExpression;
  }

  // Index expression support for ptr[index] = value
  void SetIndexExpression(std::shared_ptr<QExpression> expr) {
    m_IndexExpression = expr;
  }

  std::shared_ptr<QExpression> GetIndexExpression() const {
    return m_IndexExpression;
  }

  bool HasIndex() const { return m_IndexExpression != nullptr; }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    if (m_ValueExpression)
      m_ValueExpression->CheckForErrors(collector);
    if (m_IndexExpression)
      m_IndexExpression->CheckForErrors(collector);
  }

  // Array initializer support for ptr = {1, 2, 3, 4, 5}
  void
  SetArrayInitializer(const std::vector<std::shared_ptr<QExpression>> &exprs) {
    m_ArrayInitializer = exprs;
  }

  const std::vector<std::shared_ptr<QExpression>> &GetArrayInitializer() const {
    return m_ArrayInitializer;
  }

  bool HasArrayInitializer() const { return !m_ArrayInitializer.empty(); }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Assign: " << m_VariableName;
    if (m_IndexExpression) {
      std::cout << "[";
      const auto &idxElems = m_IndexExpression->GetElements();
      for (const auto &e : idxElems) {
        std::cout << e.value << " ";
      }
      std::cout << "]";
    }
    std::cout << " = ";
    if (HasArrayInitializer()) {
      std::cout << "{" << m_ArrayInitializer.size() << " elements}";
    } else if (m_ValueExpression) {
      const auto &elems = m_ValueExpression->GetElements();
      for (const auto &e : elems) {
        std::cout << e.value << " ";
      }
    }
    std::cout << std::endl;
  }

private:
  std::string m_VariableName;
  std::shared_ptr<QExpression> m_ValueExpression;
  std::shared_ptr<QExpression> m_IndexExpression; // For ptr[index] = value
  std::vector<std::shared_ptr<QExpression>> m_ArrayInitializer; // For {1,2,3}
};
