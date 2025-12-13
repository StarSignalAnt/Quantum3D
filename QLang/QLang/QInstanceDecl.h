#pragma once

#include "QExpression.h"
#include "QNode.h"
#include <iostream>
#include <memory>
#include <string>

// Forward declarations
class QClass;

// QInstanceDecl - represents a class instance declaration (e.g., Test t1 = new
// Test();)
class QInstanceDecl : public QNode {
public:
  QInstanceDecl(const std::string &className, const std::string &instanceName)
      : m_ClassName(className), m_InstanceName(instanceName) {
    std::cout << "[DEBUG] QInstanceDecl created: " << className << " "
              << instanceName << std::endl;
  }

  std::string GetName() const override { return m_InstanceName; }
  std::string GetClassName() const { return m_ClassName; }
  std::string GetInstanceName() const { return m_InstanceName; }

  void SetConstructorArgs(std::shared_ptr<QExpression> args) {
    m_ConstructorArgs = args;
  }

  std::shared_ptr<QExpression> GetConstructorArgs() const {
    return m_ConstructorArgs;
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "InstanceDecl: " << m_ClassName << " " << m_InstanceName
              << " = new " << m_ClassName << "(";
    if (m_ConstructorArgs) {
      const auto &elems = m_ConstructorArgs->GetElements();
      for (size_t i = 0; i < elems.size(); i++) {
        std::cout << elems[i].value;
        if (i < elems.size() - 1)
          std::cout << " ";
      }
    }
    std::cout << ")" << std::endl;
  }

private:
  std::string m_ClassName;
  std::string m_InstanceName;
  std::shared_ptr<QExpression> m_ConstructorArgs;
};
