#pragma once

#include "QExpression.h"
#include "QNode.h"
#include "QParameters.h"
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

  void SetConstructorArgs(std::shared_ptr<QParameters> args) {
    m_ConstructorArgs = args;
  }

  std::shared_ptr<QParameters> GetConstructorArgs() const {
    return m_ConstructorArgs;
  }

  // Generic type arguments (e.g., List<int32> -> ["int32"])
  void SetTypeArguments(const std::vector<std::string> &args) {
    m_TypeArguments = args;
    std::cout << "[DEBUG] QInstanceDecl(" << m_InstanceName
              << ") - type args: ";
    for (size_t i = 0; i < args.size(); i++) {
      std::cout << args[i];
      if (i < args.size() - 1)
        std::cout << ", ";
    }
    std::cout << std::endl;
  }

  const std::vector<std::string> &GetTypeArguments() const {
    return m_TypeArguments;
  }

  bool HasTypeArguments() const { return !m_TypeArguments.empty(); }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "InstanceDecl: " << m_ClassName;
    if (HasTypeArguments()) {
      std::cout << "<";
      for (size_t i = 0; i < m_TypeArguments.size(); i++) {
        std::cout << m_TypeArguments[i];
        if (i < m_TypeArguments.size() - 1)
          std::cout << ", ";
      }
      std::cout << ">";
    }
    std::cout << " " << m_InstanceName << " = new " << m_ClassName;
    if (HasTypeArguments()) {
      std::cout << "<";
      for (size_t i = 0; i < m_TypeArguments.size(); i++) {
        std::cout << m_TypeArguments[i];
        if (i < m_TypeArguments.size() - 1)
          std::cout << ", ";
      }
      std::cout << ">";
    }
    std::cout << "(";
    if (m_ConstructorArgs) {
      const auto &params = m_ConstructorArgs->GetParameters();
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
  std::string m_ClassName;
  std::string m_InstanceName;
  std::shared_ptr<QParameters> m_ConstructorArgs;
  std::vector<std::string> m_TypeArguments; // Concrete types for generics
};
