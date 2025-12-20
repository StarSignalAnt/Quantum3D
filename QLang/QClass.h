#pragma once

#include "QActionNode.h"
#include "QMethod.h"
#include "QVariableDecl.h"
#include "Tokenizer.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>


// QClass - represents a class definition with instance variables and methods
class QClass : public QActionNode {
public:
  QClass(const std::string &name) : m_Name(name) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QClass created: " << name << std::endl;
#endif
  }

  std::string GetName() const override { return m_Name; }

  // Generic type parameters (e.g., T, K, V)
  void SetTypeParameters(const std::vector<std::string> &params) {
    m_TypeParameters = params;
    std::cout << "[DEBUG] QClass(" << m_Name << ") - set type parameters: ";
    for (size_t i = 0; i < params.size(); i++) {
      std::cout << params[i];
      if (i < params.size() - 1)
        std::cout << ", ";
    }
    std::cout << std::endl;
  }

  const std::vector<std::string> &GetTypeParameters() const {
    return m_TypeParameters;
  }

  bool IsGeneric() const { return !m_TypeParameters.empty(); }

  // Parent class for inheritance (e.g., class Dog(Animal))
  void SetParentClass(const std::string &parentName) {
    m_ParentClassName = parentName;
    std::cout << "[DEBUG] QClass(" << m_Name
              << ") - set parent class: " << parentName << std::endl;
  }

  const std::string &GetParentClassName() const { return m_ParentClassName; }

  bool HasParent() const { return !m_ParentClassName.empty(); }

  void AddMember(std::shared_ptr<QVariableDecl> member) {
    m_Members.push_back(member);
    std::cout << "[DEBUG] QClass(" << m_Name
              << ") - added member: " << member->GetName() << std::endl;
  }

  const std::vector<std::shared_ptr<QVariableDecl>> &GetMembers() const {
    return m_Members;
  }

  void AddMethod(std::shared_ptr<QMethod> method) {
    m_Methods.push_back(method);
    std::cout << "[DEBUG] QClass(" << m_Name
              << ") - added method: " << method->GetName() << std::endl;
  }

  const std::vector<std::shared_ptr<QMethod>> &GetMethods() const {
    return m_Methods;
  }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    for (const auto &member : m_Members) {
      if (member)
        member->CheckForErrors(collector);
    }
    for (const auto &method : m_Methods) {
      if (method)
        method->CheckForErrors(collector);
    }
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Class: " << m_Name;
    if (HasParent()) {
      std::cout << " extends " << m_ParentClassName;
    }
    if (IsGeneric()) {
      std::cout << "<";
      for (size_t i = 0; i < m_TypeParameters.size(); i++) {
        std::cout << m_TypeParameters[i];
        if (i < m_TypeParameters.size() - 1)
          std::cout << ", ";
      }
      std::cout << ">";
    }
    std::cout << " {" << std::endl;

    // Print members
    for (const auto &member : m_Members) {
      member->Print(indent + 1);
    }

    // Print methods
    for (const auto &method : m_Methods) {
      method->Print(indent + 1);
    }

    PrintIndent(indent);
    std::cout << "}" << std::endl;
  }

private:
  std::string m_Name;
  std::string m_ParentClassName;             // Parent class for inheritance
  std::vector<std::string> m_TypeParameters; // Generic type params (T, K, V)
  std::vector<std::shared_ptr<QVariableDecl>> m_Members;
  std::vector<std::shared_ptr<QMethod>> m_Methods;
};
