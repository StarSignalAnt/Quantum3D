#pragma once

#include "QExpression.h"
#include "QNode.h"
#include "Tokenizer.h"
#include <iostream>
#include <memory>
#include <string>

// QVariableDecl - represents a variable declaration (e.g., int age = 43;)
class QVariableDecl : public QNode {
public:
  QVariableDecl(TokenType varType, const std::string &name)
      : m_VarType(varType), m_Name(name) {
    std::cout << "[DEBUG] QVariableDecl created: " << name << std::endl;
  }

  std::string GetName() const override { return m_Name; }
  TokenType GetVarType() const { return m_VarType; }

  void SetInitializer(std::shared_ptr<QExpression> expr) {
    m_Initializer = expr;
  }

  std::shared_ptr<QExpression> GetInitializer() const { return m_Initializer; }
  bool HasInitializer() const { return m_Initializer != nullptr; }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "VarDecl: " << GetVarTypeName() << " " << m_Name;
    if (m_Initializer) {
      std::cout << " = ";
      const auto &elems = m_Initializer->GetElements();
      for (const auto &e : elems) {
        std::cout << e.value << " ";
      }
    }
    std::cout << std::endl;
  }

private:
  TokenType m_VarType;
  std::string m_Name;
  std::shared_ptr<QExpression> m_Initializer;

  std::string GetVarTypeName() const {
    switch (m_VarType) {
    case TokenType::T_INT32:
      return "int32";
    case TokenType::T_INT64:
      return "int64";
    case TokenType::T_FLOAT32:
      return "float32";
    case TokenType::T_FLOAT64:
      return "float64";
    case TokenType::T_SHORT:
      return "short";
    case TokenType::T_STRING_TYPE:
      return "string";
    case TokenType::T_BOOL:
      return "bool";
    default:
      return "unknown";
    }
  }
};
