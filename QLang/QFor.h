#pragma once

#include "QCode.h"
#include "QExpression.h"
#include "QNode.h"
#include "Tokenizer.h"
#include <iostream>
#include <memory>
#include <string>

class QFor : public QNode {
public:
  QFor(const std::string &varName)
      : m_VarName(varName), m_VarType(TokenType::T_UNKNOWN),
        m_HasDeclaredType(false) {}

  std::string GetName() const override { return "For"; }

  void SetRange(std::shared_ptr<QExpression> start,
                std::shared_ptr<QExpression> end,
                std::shared_ptr<QExpression> step) {
    m_Start = start;
    m_End = end;
    m_Step = step;
  }

  void SetBody(std::shared_ptr<QCode> body) { m_Body = body; }

  // Set the declared type for the loop variable
  void SetVarType(TokenType type) {
    m_VarType = type;
    m_HasDeclaredType = true;
  }

  const std::string &GetVarName() const { return m_VarName; }
  TokenType GetVarType() const { return m_VarType; }
  bool HasDeclaredType() const { return m_HasDeclaredType; }
  std::shared_ptr<QExpression> GetStart() const { return m_Start; }
  std::shared_ptr<QExpression> GetEnd() const { return m_End; }
  std::shared_ptr<QExpression> GetStep() const { return m_Step; }
  std::shared_ptr<QCode> GetBody() const { return m_Body; }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "For loop (var: " << m_VarName;
    if (m_HasDeclaredType) {
      std::cout << ", type: " << static_cast<int>(m_VarType);
    }
    std::cout << ")" << std::endl;

    PrintIndent(indent + 1);
    std::cout << "Start:" << std::endl;
    if (m_Start)
      m_Start->Print(indent + 2);

    PrintIndent(indent + 1);
    std::cout << "End:" << std::endl;
    if (m_End)
      m_End->Print(indent + 2);

    if (m_Step) {
      PrintIndent(indent + 1);
      std::cout << "Step:" << std::endl;
      m_Step->Print(indent + 2);
    }

    PrintIndent(indent + 1);
    std::cout << "Body:" << std::endl;
    if (m_Body)
      m_Body->Print(indent + 2);
  }

private:
  std::string m_VarName;
  TokenType m_VarType;
  bool m_HasDeclaredType;
  std::shared_ptr<QExpression> m_Start;
  std::shared_ptr<QExpression> m_End;
  std::shared_ptr<QExpression> m_Step;
  std::shared_ptr<QCode> m_Body;
};
