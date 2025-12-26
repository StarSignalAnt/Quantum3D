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
std::string QLVM::s_TargetTriple = "";
std::string QLVM::s_DataLayoutStr = "";

QLVM::LLVMState::LLVMState() : Builder(Context) {
  Module = std::make_unique<llvm::Module>("QLangJIT", Context);
}

QLVM::LLVMState::~LLVMState() {}

// Explicitly initialize all targets to ensure linker pulls them in
void QLVM::InitLLVM() {
  // Check if targets are actually registered
  bool targetsRegistered = (llvm::TargetRegistry::targets().begin() !=
                            llvm::TargetRegistry::targets().end());

  if (s_Initialized && targetsRegistered)
    return;

  if (s_Initialized && !targetsRegistered) {
    std::cerr << "[WARNING] QLVM: s_Initialized is true but no targets "
                 "registered! Re-initializing..."
              << std::endl;
    // Reset initialized flag to force re-init
    s_Initialized = false;
  }

#if QLANG_DEBUG
  std::cout << "[DEBUG] Initializing LLVM..." << std::endl;
#endif

  // Initialize all available targets using standard macros
  // This will call C-API functions (likely in DLL), but /WHOLEARCHIVE on static
  // libs will ensure the static registry is ALSO populated.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  // Force the linker to include MCJIT and Interpreter symbols
  LLVMLinkInMCJIT();
  LLVMLinkInInterpreter();

  // Load symbols from the host process so JIT can resolve native functions
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  s_State = std::make_unique<LLVMState>();
  s_Initialized = true;

  // Verify targets again
  if (llvm::TargetRegistry::targets().begin() ==
      llvm::TargetRegistry::targets().end()) {
    std::cerr << "[CRITICAL] QLVM: Target registry is still empty after "
                 "initialization!"
              << std::endl;
  }

  // Set target triple and data layout for the initial module
  std::string err;
  auto triple = llvm::sys::getDefaultTargetTriple();

  // CRITICAL: Set the target triple on the module - required for MCJIT
  s_State->Module->setTargetTriple(triple);
  s_TargetTriple = triple;

  auto target = llvm::TargetRegistry::lookupTarget(triple, err);
  if (target) {
    auto *targetMachine = target->createTargetMachine(
        triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::Static);
    if (targetMachine) {
      auto dataLayout = targetMachine->createDataLayout();
      s_State->Module->setDataLayout(dataLayout);
      s_DataLayoutStr = dataLayout.getStringRepresentation();
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
  if (!s_Initialized) {
    std::cerr << "[WARNING] QLVM::CreateNewModule - LLVM not initialized, "
                 "calling InitLLVM()"
              << std::endl;
    InitLLVM();
  }

  s_State->Module =
      std::make_unique<llvm::Module>("QLangJIT", s_State->Context);

  // Apply cached target triple and data layout
  if (!s_TargetTriple.empty()) {
    s_State->Module->setTargetTriple(s_TargetTriple);
  }

  if (!s_DataLayoutStr.empty()) {
    s_State->Module->setDataLayout(llvm::DataLayout(s_DataLayoutStr));
  } else {
    // Fallback if InitLLVM didn't set it (shouldn't happen)
    std::string err;
    auto triple = llvm::sys::getDefaultTargetTriple();
    if (s_TargetTriple.empty())
      s_State->Module->setTargetTriple(triple);

    auto target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (target) {
      auto *targetMachine = target->createTargetMachine(
          triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::Static);
      if (targetMachine) {
        s_State->Module->setDataLayout(targetMachine->createDataLayout());
        delete targetMachine;
      }
    } else {
      std::cerr << "[ERROR] QLVM::CreateNewModule - failed to lookup target "
                   "for triple '"
                << triple << "': " << err << std::endl;
    }
  }
}

void QLVM::SetModule(std::unique_ptr<llvm::Module> module) {
  s_State->Module = std::move(module);
}
