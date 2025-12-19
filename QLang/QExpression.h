#pragma once

#include "QActionNode.h"
#include "QError.h"
#include "Tokenizer.h"
#include <iostream>
#include <vector>

// QExpression - a list of expression elements (tokens)
class QExpression : public QActionNode {
public:
  QExpression() { std::cout << "[DEBUG] QExpression created" << std::endl; }

  std::string GetName() const override { return "QExpression"; }

  void AddElement(const Token &token) {
    m_Elements.push_back(token);
    std::cout << "[DEBUG] QExpression - added element: " << token.value
              << std::endl;
  }

  const std::vector<Token> &GetElements() const { return m_Elements; }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    if (m_Elements.empty())
      return;

    auto IsValue = [](TokenType type) {
      return type == TokenType::T_IDENTIFIER || type == TokenType::T_INTEGER ||
             type == TokenType::T_FLOAT || type == TokenType::T_STRING ||
             type == TokenType::T_TRUE || type == TokenType::T_FALSE ||
             type == TokenType::T_NULL || type == TokenType::T_RPAREN ||
             type == TokenType::T_RBRACKET;
    };

    for (size_t i = 0; i < m_Elements.size(); ++i) {
      const Token &token = m_Elements[i];

      // Check for two values in a row (Missing Operator)
      if (i > 0) {
        const Token &prev = m_Elements[i - 1];
        if (IsValue(prev.type) && IsValue(token.type)) {
          // It's only an error if the second token is NOT a closing punctuation
          // (because '5 )' is a valid end of sub-expression, but '(a) b' or '5
          // 5' is not)
          if (token.type != TokenType::T_RPAREN &&
              token.type != TokenType::T_RBRACKET) {
            collector->ReportError(QErrorSeverity::Error,
                                   "Expected operator between values",
                                   token.line, token.column,
                                   static_cast<int>(token.value.length()));
          }
        }
      }

      if (token.type == TokenType::T_OPERATOR) {
        // Check for operator at the end
        if (i == m_Elements.size() - 1) {
          // Unary Postfix IS allowed at end? e.g. i++
          bool isPostfix = (token.value == "++" || token.value == "--");
          if (!isPostfix) {
            collector->ReportError(QErrorSeverity::Error,
                                   "Expression cannot end with operator '" +
                                       token.value + "'",
                                   token.line, token.column,
                                   static_cast<int>(token.value.length()));
          }
        }

        // Check validity based on previous token
        bool requiresUnary =
            (i == 0) || (m_Elements[i - 1].type == TokenType::T_OPERATOR) ||
            (m_Elements[i - 1].type == TokenType::T_LPAREN);

        if (requiresUnary) {
          bool isUnary = (token.value == "!" || token.value == "-");
          // Prefix ++/-- also allowed? C++ allows ++i.
          // Let's assume yes.
          if (token.value == "++" || token.value == "--")
            isUnary = true;

          if (!isUnary) {
            if (i == 0) {
              collector->ReportError(QErrorSeverity::Error,
                                     "Expression cannot start with operator '" +
                                         token.value + "'",
                                     token.line, token.column,
                                     static_cast<int>(token.value.length()));
            } else {
              collector->ReportError(
                  QErrorSeverity::Error,
                  "Unexpected operator '" + token.value + "'", token.line,
                  token.column, static_cast<int>(token.value.length()));
            }
          }
        } else {
          // Operator follows a value (Binary or Postfix)
          bool isPostfix = (token.value == "++" || token.value == "--");
          if (isPostfix) {
            // If postfix, NEXT token cannot be a Value.
            // e.g. i++ 100
            if (i + 1 < m_Elements.size()) {
              const Token &next = m_Elements[i + 1];
              if (IsValue(next.type)) {
                collector->ReportError(
                    QErrorSeverity::Error,
                    "Unexpected value after postfix operator", next.line,
                    next.column, static_cast<int>(next.value.length()));
              }
            }
          }
        }
      }
    }
  }

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
