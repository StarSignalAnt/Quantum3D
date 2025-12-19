#pragma once

#include "QExpression.h"
#include "QNode.h"
#include <iostream>
#include <memory>

// QReturn - represents a return statement (e.g., return 50;)
class QReturn : public QNode {
public:
  QReturn() { std::cout << "[DEBUG] QReturn created" << std::endl; }

  std::string GetName() const override { return "return"; }

  void SetExpression(std::shared_ptr<QExpression> expr) { m_Expression = expr; }

  std::shared_ptr<QExpression> GetExpression() const { return m_Expression; }

  bool HasExpression() const { return m_Expression != nullptr; }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Return";
    if (m_Expression) {
      std::cout << ": ";
      const auto &elems = m_Expression->GetElements();
      for (const auto &e : elems) {
        std::cout << e.value << " ";
      }
    }
    std::cout << std::endl;
  }

private:
  std::shared_ptr<QExpression> m_Expression;
};
