#include "QLangDomain.h"
#include <iostream>
#include <memory>
#include <variant>
#include <vector>

#include "GraphNode.h"
#include "Parser.h"

#include "QError.h"

#include "Tokenizer.h"
#include <filesystem>

// Global Funcs

#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/DerivedTypes.h>

// Force MCJIT to be linked - User indicates this fixes the issue in CLI
namespace {
struct ForceMCJITLink {
  ForceMCJITLink() { LLVMLinkInMCJIT(); }
} g_ForceMCJIT;
} // namespace

QLangDomain *QLangDomain::m_QLang = nullptr;

QLangDomain::QLangDomain(const std::string &projectPath) {
  m_QLang = this;
  std::cout << "Creating QLang domain" << std::endl;

  auto errorCollector = std::make_shared<QErrorCollector>();

  QLVM::InitLLVM();
  m_Context = std::make_shared<QLVMContext>();

  m_Runner = std::make_shared<QJitRunner>(m_Context, errorCollector);
  // QJitRunner runner(m_Context, errorCollector);

  m_Runner->SetBasePath("engine/qlang/classes");



  m_Runner->BuildModule("engine/qlang/classes/Vec3.q");

  m_Runner->BuildModule("engine/qlang/classes/matrix.q");

  m_Runner->BuildModule("engine/qlang/classes/gamenode.q");

  int b = 5;

  // exit(1);
}

std::string GetFileStem(const std::string &path) {
  return std::filesystem::path(path).stem().string();
}

void QLangDomain::CompileScript(std::string path) {

  auto cls_name = GetFileStem(path);

  m_Runner->RunScript(path);

  int b = 5;
}
