#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>

class QLVM {
public:
  static void InitLLVM();
  static llvm::LLVMContext &GetContext();
  static llvm::IRBuilder<> &GetBuilder();
  static llvm::Module *GetModule();
  static std::unique_ptr<llvm::Module> TakeModule();
  static void CreateNewModule();
  static void SetModule(std::unique_ptr<llvm::Module> module);

private:
  struct LLVMState {
    llvm::LLVMContext Context;
    llvm::IRBuilder<> Builder;
    std::unique_ptr<llvm::Module> Module;

    LLVMState();
    ~LLVMState();
  };

  static std::unique_ptr<LLVMState> s_State;
  static bool s_Initialized;
};
