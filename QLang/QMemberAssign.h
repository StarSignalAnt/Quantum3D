#pragma once

#include "QExpression.h"
#include "QNode.h"
#include <iostream>
#include <memory>
#include <string>

// QMemberAssign - represents a member field assignment (e.g., t1.num = 90;)
class QMemberAssign : public QNode {
public:
  QMemberAssign(const std::string &instanceName, const std::string &memberName)
      : m_InstanceName(instanceName), m_MemberName(memberName) {
    std::cout << "[DEBUG] QMemberAssign created: " << instanceName << "."
              << memberName << std::endl;
  }

  std::string GetName() const override {
    return m_InstanceName + "." + m_MemberName;
  }

  const std::string &GetInstanceName() const { return m_InstanceName; }
  const std::string &GetMemberName() const { return m_MemberName; }

  void SetValueExpression(std::shared_ptr<QExpression> expr) {
    m_ValueExpression = expr;
  }
  std::shared_ptr<QExpression> GetValueExpression() const {
    return m_ValueExpression;
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "MemberAssign: " << m_InstanceName << "." << m_MemberName
              << " = ";
    if (m_ValueExpression) {
      const auto &elems = m_ValueExpression->GetElements();
      for (const auto &e : elems) {
        std::cout << e.value << " ";
      }
    }
    std::cout << std::endl;
  }

private:
  std::string m_InstanceName;
  std::string m_MemberName;
  std::shared_ptr<QExpression> m_ValueExpression;
};
