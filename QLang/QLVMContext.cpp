#include "QLVMContext.h"
#include "QLVM.h"
#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/DynamicLibrary.h>

QLVMContext::QLVMContext() {}

QLVMContext::~QLVMContext() {}

void QLVMContext::AddFunc(const std::string &name, void *funcPtr,
                          llvm::FunctionType *funcType) {
  auto &context = QLVM::GetContext();
  auto *module = QLVM::GetModule();

  // Create the function declaration in the LLVM module
  llvm::Function *func = llvm::Function::Create(
      funcType, llvm::Function::ExternalLinkage, name, module);

  m_LLVMFunctions[name] = func;
  m_FunctionTypes[name] = funcType;
  m_FunctionPtrs[name] = funcPtr;

  // Register the symbol globally so MCJIT can find it
  llvm::sys::DynamicLibrary::AddSymbol(name, funcPtr);
  std::cout << "[DEBUG] QLVMContext: Registered symbol '" << name
            << "' at address " << funcPtr << std::endl;
}

void *QLVMContext::GetFuncPtr(const std::string &name) const {
  auto it = m_FunctionPtrs.find(name);
  if (it != m_FunctionPtrs.end()) {
    return it->second;
  }
  return nullptr;
}

llvm::Function *QLVMContext::GetLLVMFunc(const std::string &name) const {
  auto it = m_LLVMFunctions.find(name);
  if (it != m_LLVMFunctions.end()) {
    // Check if the function belongs to the currently active module
    auto *module = QLVM::GetModule();
    if (it->second->getParent() != module) {
      // It's from a different module (likely the main program)
      // Check if it already exists in the current module
      llvm::Function *existingFunc = module->getFunction(name);
      if (existingFunc) {
        return existingFunc;
      }

      // Redeclare it in the current module
      auto typeIt = m_FunctionTypes.find(name);
      if (typeIt != m_FunctionTypes.end()) {
        llvm::Function *newFunc = llvm::Function::Create(
            typeIt->second, llvm::Function::ExternalLinkage, name, module);
        return newFunc;
      }
    }
    return it->second;
  }
  return nullptr;
}
