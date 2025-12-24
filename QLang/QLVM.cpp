#include "QLVM.h"
#include <iostream>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h> // Newer LLVM location

std::unique_ptr<QLVM::LLVMState> QLVM::s_State = nullptr;
bool QLVM::s_Initialized = false;

QLVM::LLVMState::LLVMState() : Builder(Context) {
  Module = std::make_unique<llvm::Module>("QLangJIT", Context);
}

QLVM::LLVMState::~LLVMState() {}

void QLVM::InitLLVM() {
  if (s_Initialized)
    return;

#if QLANG_DEBUG
  std::cout << "[DEBUG] Initializing LLVM..." << std::endl;
#endif

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  // Force the linker to include MCJIT symbols - THIS IS CRITICAL
  LLVMLinkInMCJIT();

  // Load symbols from the host process so JIT can resolve native functions
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  s_State = std::make_unique<LLVMState>();
  s_Initialized = true;

  // Set data layout for the initial module
  std::string err;
  auto triple = llvm::sys::getDefaultTargetTriple();
  auto target = llvm::TargetRegistry::lookupTarget(triple, err);
  if (target) {
    auto *targetMachine = target->createTargetMachine(
        triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::Static);
    if (targetMachine) {
      s_State->Module->setDataLayout(targetMachine->createDataLayout());
      delete targetMachine;
    }
  }

#if QLANG_DEBUG
  std::cout << "[DEBUG] LLVM Initialized successfully." << std::endl;
#endif
}

llvm::LLVMContext &QLVM::GetContext() { return s_State->Context; }

llvm::IRBuilder<> &QLVM::GetBuilder() { return s_State->Builder; }

llvm::Module *QLVM::GetModule() { return s_State->Module.get(); }

std::unique_ptr<llvm::Module> QLVM::TakeModule() {
  auto oldModule = std::move(s_State->Module);
  CreateNewModule();
  return oldModule;
}

void QLVM::CreateNewModule() {
  s_State->Module =
      std::make_unique<llvm::Module>("QLangJIT", s_State->Context);

  // Apply same layout to the new module
  std::string err;
  auto triple = llvm::sys::getDefaultTargetTriple();
  auto target = llvm::TargetRegistry::lookupTarget(triple, err);
  if (target) {
    auto *targetMachine = target->createTargetMachine(
        triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::Static);
    if (targetMachine) {
      s_State->Module->setDataLayout(targetMachine->createDataLayout());
      delete targetMachine;
    }
  }
}

void QLVM::SetModule(std::unique_ptr<llvm::Module> module) {
  s_State->Module = std::move(module);
}
