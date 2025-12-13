#pragma once

#include "QAction.h"
#include "QClass.h"
#include "QCode.h"
#include <iostream>
#include <memory>
#include <vector>

// QProgram - top level action node
class QProgram : public QAction {
public:
  QProgram() {
    std::cout << "[DEBUG] QProgram created" << std::endl;
    m_Code = std::make_shared<QCode>();
  }

  std::string GetName() const override { return "QProgram"; }

  std::shared_ptr<QCode> GetCode() { return m_Code; }

  void AddClass(std::shared_ptr<QClass> cls) { m_Classes.push_back(cls); }

  const std::vector<std::shared_ptr<QClass>> &GetClasses() const {
    return m_Classes;
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "QProgram {" << std::endl;

    // Print classes
    if (!m_Classes.empty()) {
      PrintIndent(indent + 1);
      std::cout << "Classes:" << std::endl;
      for (const auto &cls : m_Classes) {
        cls->Print(indent + 2);
      }
    }

    // Print code
    m_Code->Print(indent + 1);
    PrintIndent(indent);
    std::cout << "}" << std::endl;
  }

private:
  std::shared_ptr<QCode> m_Code;
  std::vector<std::shared_ptr<QClass>> m_Classes;
};
