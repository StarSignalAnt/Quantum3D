#pragma once

#include "QAction.h"
#include "QMethod.h"
#include "QVariableDecl.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// QClass - represents a class definition with instance variables and methods
class QClass : public QAction {
public:
  QClass(const std::string &name) : m_Name(name) {
    std::cout << "[DEBUG] QClass created: " << name << std::endl;
  }

  std::string GetName() const override { return m_Name; }

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

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "Class: " << m_Name << " {" << std::endl;

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
  std::vector<std::shared_ptr<QVariableDecl>> m_Members;
  std::vector<std::shared_ptr<QMethod>> m_Methods;
};
