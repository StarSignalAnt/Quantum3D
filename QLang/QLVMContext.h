#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace llvm {
class Function;
class FunctionType;
} // namespace llvm

class QLVMContext {
public:
  QLVMContext();
  ~QLVMContext();

  // Register a native function with LLVM
  void AddFunc(const std::string &name, void *funcPtr,
               llvm::FunctionType *funcType);

  // Get the native function pointer
  void *GetFuncPtr(const std::string &name) const;

  // Get the LLVM function declaration
  llvm::Function *GetLLVMFunc(const std::string &name) const;

  // Clear cached LLVM functions (used when module changes)
  void ResetCache();

private:
  // Register all built-in functions automatically
  void RegisterBuiltinFunctions();
  mutable std::unordered_map<std::string, llvm::Function *> m_LLVMFunctions;
  std::unordered_map<std::string, llvm::FunctionType *> m_FunctionTypes;
  std::unordered_map<std::string, void *> m_FunctionPtrs;
};
