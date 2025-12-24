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
    // We found it in the cache.
    // Ideally we should check if it belongs to the current module, but
    // accessing it->second->getParent() is unsafe if the module was deleted.
    // The caller (QJitRunner) is responsible for calling ResetCache() when
    // cached functions become invalid.
    return it->second;
  }

  // Not in cache, or cache was cleared.
  // Check if we have the type info to recreate it.
  auto typeIt = m_FunctionTypes.find(name);
  if (typeIt != m_FunctionTypes.end()) {
    auto *module = QLVM::GetModule();

    // Check if it already exists in the module (e.g. linked in or previously
    // created)
    llvm::Function *existingFunc = module->getFunction(name);
    if (existingFunc) {
      m_LLVMFunctions[name] = existingFunc;
      return existingFunc;
    }

    // Create fresh declaration
    llvm::Function *newFunc = llvm::Function::Create(
        typeIt->second, llvm::Function::ExternalLinkage, name, module);
    m_LLVMFunctions[name] = newFunc;
    return newFunc;
  }

  return nullptr;
}

void QLVMContext::ResetCache() { m_LLVMFunctions.clear(); }
