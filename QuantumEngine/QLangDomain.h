#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include "QContext.h"
#include "GraphNode.h"

class QContext;
class QClassInstance;

class QRunner;

class QLangDomain {
public:
  QLangDomain();
  bool LoadAndRegister(std::string path);
  int LoadAndRegisterFolder(const std::string &folderPath);
  std::shared_ptr<QClassInstance> LoadClass(std::string path,Quantum::GraphNode* node);
  void RunMethod(std::shared_ptr<QClassInstance> inst, const std::string& meth, const std::vector<QValue>& args = {});

  static QLangDomain* m_QLang;

private:
  std::shared_ptr<QContext> m_Context;
  std::shared_ptr<QRunner> m_Runner;
};
