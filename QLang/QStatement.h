#pragma once

#include "QNode.h"
#include "QParameters.h"
#include <iostream>
#include <memory>
#include <string>

// QStatement - represents a statement with name and parameters
class QStatement : public QNode {
public:
  QStatement(const std::string &name) : m_Name(name) {
    std::cout << "[DEBUG] QStatement created: " << name << std::endl;
  }

  std::string GetName() const override { return m_Name; }

  void SetParameters(std::shared_ptr<QParameters> params) {
    m_Parameters = params;
  }

  std::shared_ptr<QParameters> GetParameters() const { return m_Parameters; }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    if (m_Parameters)
      m_Parameters->CheckForErrors(collector);
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Statement: " << m_Name << std::endl;
    if (m_Parameters) {
      m_Parameters->Print(indent + 1);
    }
  }

private:
  std::string m_Name;
  std::shared_ptr<QParameters> m_Parameters;
};
