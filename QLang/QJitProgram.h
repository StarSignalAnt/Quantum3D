#pragma once

#include <memory>

namespace llvm {
class ExecutionEngine;
class Module;
} // namespace llvm

class QJitProgram {
public:
  QJitProgram(std::unique_ptr<llvm::Module> module);
  ~QJitProgram();

  void Run();

private:
  llvm::ExecutionEngine *m_Engine = nullptr;
};
