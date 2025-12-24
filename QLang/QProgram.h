#pragma once

#include "QActionNode.h"
#include "QClass.h"
#include "QCode.h"
#include <iostream>
#include <memory>
#include <vector>

// QProgram - top level action node
class QProgram : public QActionNode {
public:
  QProgram() {
#if QLANG_DEBUG
    std::cout << "[DEBUG] QProgram created" << std::endl;
#endif
    m_Code = std::make_shared<QCode>();
  }

  std::string GetName() const override { return "QProgram"; }

  std::shared_ptr<QCode> GetCode() { return m_Code; }

  void AddClass(std::shared_ptr<QClass> cls) { m_Classes.push_back(cls); }

  const std::vector<std::shared_ptr<QClass>> &GetClasses() const {
    return m_Classes;
  }

  // Import handling
  void AddImport(const std::string &moduleName) {
    m_Imports.push_back(moduleName);
  }

  const std::vector<std::string> &GetImports() const { return m_Imports; }

  void CheckForErrors(std::shared_ptr<QErrorCollector> collector) override {
    for (const auto &cls : m_Classes) {
      if (cls)
        cls->CheckForErrors(collector);
    }
    if (m_Code)
      m_Code->CheckForErrors(collector);
  }

  void Print(int indent = 0) const override {
    PrintIndent(indent);
    std::cout << "QProgram {" << std::endl;

    // Print imports
    if (!m_Imports.empty()) {
      PrintIndent(indent + 1);
      std::cout << "Imports:" << std::endl;
      for (const auto &imp : m_Imports) {
        PrintIndent(indent + 2);
        std::cout << imp << std::endl;
      }
    }

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
  std::vector<std::string> m_Imports;
};
