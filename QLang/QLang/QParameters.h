#pragma once

#include "QAction.h"
#include "QExpression.h"
#include <iostream>
#include <memory>
#include <vector>

// QParameters - holds a list of expressions (parameters)
class QParameters : public QAction {
public:
  QParameters() { std::cout << "[DEBUG] QParameters created" << std::endl; }

  std::string GetName() const override { return "QParameters"; }

  void AddParameter(std::shared_ptr<QExpression> expr) {
    m_Parameters.push_back(expr);
    std::cout << "[DEBUG] QParameters - added parameter #"
              << m_Parameters.size() << std::endl;
  }

  const std::vector<std::shared_ptr<QExpression>> &GetParameters() const {
    return m_Parameters;
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Parameters (" << m_Parameters.size() << "):" << std::endl;
    for (const auto &param : m_Parameters) {
      param->Print(indent + 1);
    }
  }

private:
  std::vector<std::shared_ptr<QExpression>> m_Parameters;
};
