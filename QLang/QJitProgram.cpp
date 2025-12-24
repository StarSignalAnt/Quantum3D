#include "QJitProgram.h"
#include "QLVM.h"
#include <iostream>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>

QJitProgram::QJitProgram(std::unique_ptr<llvm::Module> module) {
  if (!module) {
    std::cerr << "[ERROR] QJitProgram: Received null module" << std::endl;
    return;
  }

  std::string err;
  llvm::EngineBuilder builder(std::move(module));
  builder.setErrorStr(&err);
  builder.setEngineKind(llvm::EngineKind::JIT);

  m_Engine = builder.create();

  if (!m_Engine) {
    std::cerr << "[ERROR] QJitProgram: Failed to create ExecutionEngine: "
              << err << std::endl;
  } else {
    std::cout << "[INFO] QJitProgram: ExecutionEngine created successfully."
              << std::endl;
  }
}

QJitProgram::~QJitProgram() {
  if (m_Engine) {
    delete m_Engine;
  }
}

void QJitProgram::Run() {
  if (!m_Engine) {
    std::cerr << "[ERROR] QJitProgram: Cannot run, engine is null" << std::endl;
    return;
  }

  uint64_t addr = m_Engine->getFunctionAddress("__qlang_global_entry");
  if (addr) {
    std::cout << "[INFO] QJitProgram: Executing __qlang_global_entry at 0x"
              << std::hex << addr << std::dec << "..." << std::endl;
    void (*entry)() = (void (*)())addr;
    try {
      entry();
    } catch (...) {
      std::cerr << "[ERROR] QJitProgram: Exception during execution!"
                << std::endl;
    }
    std::cout << "[INFO] QJitProgram: Execution finished." << std::endl;
  } else {
    std::cerr << "[ERROR] QJitProgram: Failed to resolve address for "
                 "__qlang_global_entry"
              << std::endl;
  }
}
