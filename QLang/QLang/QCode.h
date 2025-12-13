#pragma once

#include "QAction.h"
#include "QNode.h"
#include <iostream>
#include <memory>
#include <vector>

// QCode - represents a block of code containing QNodes
class QCode : public QAction {
public:
  QCode() { std::cout << "[DEBUG] QCode created" << std::endl; }

  std::string GetName() const override { return "QCode"; }

  void AddNode(std::shared_ptr<QNode> node) { m_Nodes.push_back(node); }

  const std::vector<std::shared_ptr<QNode>> &GetNodes() const {
    return m_Nodes;
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "QCode {" << std::endl;
    for (const auto &node : m_Nodes) {
      node->Print(indent + 1);
    }
    PrintIndent(indent);
    std::cout << "}" << std::endl;
  }

private:
  std::vector<std::shared_ptr<QNode>> m_Nodes;
};
