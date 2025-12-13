#pragma once

#include "QCode.h"
#include "QExpression.h"
#include "QNode.h"
#include <iostream>
#include <memory>
#include <vector>

// QIf - represents an if-elseif-else control structure
class QIf : public QNode {
public:
  QIf() { std::cout << "[DEBUG] QIf created" << std::endl; }

  std::string GetName() const override { return "If"; }

  // Set the main if condition and block
  void SetIf(std::shared_ptr<QExpression> condition,
             std::shared_ptr<QCode> block) {
    m_Condition = condition;
    m_ThenBlock = block;
  }

  // Add an elseif block
  void AddElseIf(std::shared_ptr<QExpression> condition,
                 std::shared_ptr<QCode> block) {
    m_ElseIfBlocks.push_back({condition, block});
  }

  // Set the else block
  void SetElse(std::shared_ptr<QCode> block) { m_ElseBlock = block; }

  // Getters
  std::shared_ptr<QExpression> GetCondition() const { return m_Condition; }
  std::shared_ptr<QCode> GetThenBlock() const { return m_ThenBlock; }

  const std::vector<
      std::pair<std::shared_ptr<QExpression>, std::shared_ptr<QCode>>> &
  GetElseIfBlocks() const {
    return m_ElseIfBlocks;
  }

  std::shared_ptr<QCode> GetElseBlock() const { return m_ElseBlock; }
  bool HasElse() const { return m_ElseBlock != nullptr; }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "If condition:" << std::endl;
    if (m_Condition)
      m_Condition->Print(indent + 1);

    PrintIndent(indent);
    std::cout << "Then block:" << std::endl;
    if (m_ThenBlock)
      m_ThenBlock->Print(indent + 1);

    for (const auto &pair : m_ElseIfBlocks) {
      PrintIndent(indent);
      std::cout << "ElseIf condition:" << std::endl;
      pair.first->Print(indent + 1);
      PrintIndent(indent);
      std::cout << "ElseIf block:" << std::endl;
      pair.second->Print(indent + 1);
    }

    if (m_ElseBlock) {
      PrintIndent(indent);
      std::cout << "Else block:" << std::endl;
      m_ElseBlock->Print(indent + 1);
    }
  }

private:
  std::shared_ptr<QExpression> m_Condition;
  std::shared_ptr<QCode> m_ThenBlock;

  // List of (condition, block) pairs for elseif branches
  std::vector<std::pair<std::shared_ptr<QExpression>, std::shared_ptr<QCode>>>
      m_ElseIfBlocks;

  std::shared_ptr<QCode> m_ElseBlock;
};
