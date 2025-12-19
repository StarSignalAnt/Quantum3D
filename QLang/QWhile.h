#pragma once

#include "QCode.h"
#include "QExpression.h"
#include "QNode.h"
#include <iostream>
#include <memory>

class QWhile : public QNode {
public:
  QWhile() {}

  std::string GetName() const override { return "While"; }

  void SetCondition(std::shared_ptr<QExpression> condition) {
    m_Condition = condition;
  }

  void SetBody(std::shared_ptr<QCode> body) { m_Body = body; }

  std::shared_ptr<QExpression> GetCondition() const { return m_Condition; }
  std::shared_ptr<QCode> GetBody() const { return m_Body; }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    if (m_Condition)
      m_Condition->CheckForErrors(collector);
    if (m_Body)
      m_Body->CheckForErrors(collector);
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "While loop" << std::endl;

    PrintIndent(indent + 1);
    std::cout << "Condition:" << std::endl;
    if (m_Condition)
      m_Condition->Print(indent + 2);

    PrintIndent(indent + 1);
    std::cout << "Body:" << std::endl;
    if (m_Body)
      m_Body->Print(indent + 2);
  }

private:
  std::shared_ptr<QExpression> m_Condition;
  std::shared_ptr<QCode> m_Body;
};
