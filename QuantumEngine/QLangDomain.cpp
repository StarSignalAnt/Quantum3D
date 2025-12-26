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

extern "C" void LV_GetNode(void *ptr) {
  std::cout << "[C++] TestNode called with pointer: " << ptr << std::endl;
  if (ptr) {

    return;
    // Example: interpret as int32* and print first few values
    float *iptr = static_cast<float *>(ptr);
    std::cout << "[C++] First 5 float values: ";
    for (int i = 0; i < 5; i++) {
      std::cout << iptr[i] << " ";
    }
    std::cout << std::endl;
  }
}

extern "C" void LV_Node_Turn(void *ptr, void *vec3) {
  // Vec3 layout: X, Y, Z as consecutive floats (12 bytes total)
  const float *v = static_cast<const float *>(vec3);

  // TODO: Apply rotation to the node pointed to by 'ptr'
  // For now, extract the rotation values directly from memory
  [[maybe_unused]] float x = v[0];
  [[maybe_unused]] float y = v[1];
  [[maybe_unused]] float z = v[2];



  // Example: Cast ptr to GraphNode and apply rotation
   auto* node = static_cast<Quantum::GraphNode*>(ptr);
   node->Turn(glm::vec3(x, y, z));
}

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
  auto *testNodeType = llvm::FunctionType::get(
      llvm::Type::getVoidTy(QLVM::GetContext()),
      {llvm::PointerType::getUnqual(QLVM::GetContext())}, false);
  m_Context->AddFunc("TestNode", (void *)LV_GetNode, testNodeType);

  auto *nodeTurnType = llvm::FunctionType::get(
      llvm::Type::getVoidTy(QLVM::GetContext()),
      {llvm::PointerType::getUnqual(QLVM::GetContext()),
       llvm::PointerType::getUnqual(QLVM::GetContext())},
      false);
  m_Context->AddFunc("Node_Turn", (void *)LV_Node_Turn, nodeTurnType);

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

Quantum::ScriptPair *QLangDomain::CompileScript(std::string path) {

  auto cls_name = GetFileStem(path);

  auto prog = m_Runner->RunScript(path);

  auto inst = prog->CreateClassInstance(cls_name);

  Quantum::ScriptPair *res = new Quantum::ScriptPair;

  res->ClsInstance = inst;
  res->ClsProgram = prog;

  return res;

  int b = 5;
}
