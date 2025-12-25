#pragma once

#include "QActionNode.h"
#include "QCode.h"
#include "Tokenizer.h"
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Forward declaration
class QExpression;

// QMethodParam - represents a method parameter
struct QMethodParam {
  TokenType type;
  std::string name;
  std::string typeName; // Original type name (for generics like T, K, V)
};

// QMethod - represents a class method definition
class QMethod : public QActionNode {
public:
  QMethod(const std::string &name) : m_Name(name) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QMethod created: " << name << std::endl;
#endif
    m_Body = std::make_shared<QCode>();
  }

  std::string GetName() const override { return m_Name; }

  void SetReturnType(TokenType type, const std::string &typeName = "") {
    m_ReturnType = type;
    m_ReturnTypeName = typeName;
  }
  TokenType GetReturnType() const { return m_ReturnType; }
  std::string GetReturnTypeName() const { return m_ReturnTypeName; }

  void AddParameter(TokenType type, const std::string &name,
                    const std::string &typeName = "") {
    m_Parameters.push_back({type, name, typeName});
    std::cout << "[DEBUG] QMethod(" << m_Name << ") - added param: " << name
              << " (type: " << typeName << ")" << std::endl;
  }

  const std::vector<QMethodParam> &GetParameters() const {
    return m_Parameters;
  }

  std::shared_ptr<QCode> GetBody() { return m_Body; }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    if (m_Body)
      m_Body->CheckForErrors(collector);
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Method: " << m_Name << "(";
    for (size_t i = 0; i < m_Parameters.size(); i++) {
      std::cout << GetTypeName(m_Parameters[i].type) << " "
                << m_Parameters[i].name;
      if (i < m_Parameters.size() - 1)
        std::cout << ", ";
    }
    std::cout << ")";
    if (m_IsVirtual)
      std::cout << " virtual";
    if (m_IsOverride)
      std::cout << " override";
    std::cout << std::endl;
    m_Body->Print(indent + 1);
  }

  // Virtual method support
  void SetVirtual(bool isVirtual) { m_IsVirtual = isVirtual; }
  bool IsVirtual() const { return m_IsVirtual; }
  void SetOverride(bool isOverride) { m_IsOverride = isOverride; }
  bool IsOverride() const { return m_IsOverride; }

private:
  std::string m_Name;
  TokenType m_ReturnType = TokenType::T_EOF; // T_EOF = void/no return
  std::string m_ReturnTypeName;
  std::vector<QMethodParam> m_Parameters;
  std::shared_ptr<QCode> m_Body;
  bool m_IsVirtual = false;  // True if method is virtual
  bool m_IsOverride = false; // True if method overrides a parent method

  std::string GetTypeName(TokenType type) const {
    switch (type) {
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
      return "void";
    }
  }
};
