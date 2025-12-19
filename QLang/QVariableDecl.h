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
  QVariableDecl(TokenType varType, const std::string &name,
                const std::string &typeName = "")
      : m_VarType(varType), m_Name(name), m_TypeName(typeName) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QVariableDecl created: " << name << std::endl;
#endif
  }

  std::string GetName() const override { return m_Name; }
  TokenType GetVarType() const { return m_VarType; }

  // Get the original type name string (for generics like T, K, V)
  std::string GetTypeName() const { return m_TypeName; }
  void SetTypeName(const std::string &name) { m_TypeName = name; }

  // Generic support
  void SetTypeParameters(const std::vector<std::string> &params) {
    m_TypeParams = params;
  }
  const std::vector<std::string> &GetTypeParameters() const {
    return m_TypeParams;
  }

  void SetInitializer(std::shared_ptr<QExpression> expr) {
    m_Initializer = expr;
  }

  std::shared_ptr<QExpression> GetInitializer() const { return m_Initializer; }
  bool HasInitializer() const { return m_Initializer != nullptr; }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    if (m_Initializer)
      m_Initializer->CheckForErrors(collector);
  }

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
  std::string
      m_TypeName; // Original type name string (for generics like T, K, V)
  std::vector<std::string> m_TypeParams;
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
    case TokenType::T_CPTR:
      return "cptr";
    default:
      return "unknown";
    }
  }
};
