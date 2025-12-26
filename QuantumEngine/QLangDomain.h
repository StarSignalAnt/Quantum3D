#pragma once

#include "GraphNode.h"
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "QJitProgram.h"
#include "QJClassInstance.h"
#include "QJitRunner.h"
#include "QLVM.h"
#include "QLVMContext.h"
#include "QStaticRegistry.h"

namespace Quantum {
    class ScriptPair {
    public:
        std::shared_ptr<QJClassInstance> ClsInstance;
        std::shared_ptr<QJitProgram> ClsProgram;

    };
}

class QLangDomain {
public:
  QLangDomain(const std::string &projectPath = "");
 
  std::shared_ptr<QJitRunner> GetRunner() { return m_Runner; }

  static std::string GetScriptClassName(std::shared_ptr<QJClassInstance> script);
  Quantum::ScriptPair* CompileScript(std::string file);

  static QLangDomain *m_QLang;

private:
  std::shared_ptr<QLVMContext> m_Context;
  std::shared_ptr<QJitRunner> m_Runner;
};
