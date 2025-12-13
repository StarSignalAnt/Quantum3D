#pragma once

#include "QExpression.h"
#include "QNode.h"
#include "QParameters.h"
#include <iostream>
#include <memory>
#include <string>

// QMethodCall - represents a method call on an instance (e.g., t1.TestMeth())
class QMethodCall : public QNode {
public:
  QMethodCall(const std::string &instanceName, const std::string &methodName)
      : m_InstanceName(instanceName), m_MethodName(methodName) {
    std::cout << "[DEBUG] QMethodCall created: " << instanceName << "."
              << methodName << "()" << std::endl;
  }

  std::string GetName() const override {
    return m_InstanceName + "." + m_MethodName;
  }

  const std::string &GetInstanceName() const { return m_InstanceName; }
  const std::string &GetMethodName() const { return m_MethodName; }

  void SetArguments(std::shared_ptr<QParameters> args) { m_Arguments = args; }
  std::shared_ptr<QParameters> GetArguments() const { return m_Arguments; }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "MethodCall: " << m_InstanceName << "." << m_MethodName << "(";
    if (m_Arguments) {
      const auto &params = m_Arguments->GetParameters();
      for (size_t i = 0; i < params.size(); i++) {
        const auto &elems = params[i]->GetElements();
        for (const auto &e : elems) {
          std::cout << e.value << " ";
        }
        if (i < params.size() - 1)
          std::cout << ", ";
      }
    }
    std::cout << ")" << std::endl;
  }

private:
  std::string m_InstanceName;
  std::string m_MethodName;
  std::shared_ptr<QParameters> m_Arguments;
};
