#pragma once
#include "GraphNode.h"
#include "QContext.h"
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class QContext;
class QClassInstance;

class QRunner;

class QLangDomain {
public:
  QLangDomain(const std::string &projectPath = "");
  bool LoadAndRegister(std::string path);
  int LoadAndRegisterFolder(const std::string &folderPath);
  std::shared_ptr<QClassInstance> LoadClass(std::string path,
                                            Quantum::GraphNode *node);
  void RunMethod(std::shared_ptr<QClassInstance> inst, const std::string &meth,
                 const std::vector<QValue> &args = {});
  std::shared_ptr<QRunner> GetRunner() { return m_Runner; }

  static QLangDomain *m_QLang;

private:
  std::shared_ptr<QContext> m_Context;
  std::shared_ptr<QRunner> m_Runner;
};
