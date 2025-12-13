#pragma once

#include "QAction.h"
#include "Tokenizer.h"
#include <iostream>
#include <vector>

// QExpression - a list of expression elements (tokens)
class QExpression : public QAction {
public:
  QExpression() { std::cout << "[DEBUG] QExpression created" << std::endl; }

  std::string GetName() const override { return "QExpression"; }

  void AddElement(const Token &token) {
    m_Elements.push_back(token);
    std::cout << "[DEBUG] QExpression - added element: " << token.value
              << std::endl;
  }

  const std::vector<Token> &GetElements() const { return m_Elements; }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Expression: ";
    for (size_t i = 0; i < m_Elements.size(); i++) {
      std::cout << m_Elements[i].value;
      if (i < m_Elements.size() - 1)
        std::cout << " ";
    }
    std::cout << std::endl;
  }

private:
  std::vector<Token> m_Elements;
};
