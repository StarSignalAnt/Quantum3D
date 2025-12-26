#include "QJitProgram.h"
#include "QLVM.h"
#include "QStaticRegistry.h"
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

    // Fallback to interpreter
    std::cout << "[INFO] QJitProgram: Falling back to Interpreter..."
              << std::endl;
    builder.setEngineKind(llvm::EngineKind::Interpreter);
    m_Engine = builder.create();
    if (m_Engine) {
      std::cout << "[INFO] QJitProgram: Interpreter created successfully."
                << std::endl;
    } else {
      std::cerr << "[ERROR] QJitProgram: Interpreter creation also failed: "
                << err << std::endl;
    }
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

uint64_t QJitProgram::GetFunctionAddress(const std::string &funcName) {
  if (!m_Engine) {
    return 0;
  }
  return m_Engine->getFunctionAddress(funcName);
}

void QJitProgram::RegisterClass(const std::string &className,
                                llvm::StructType *structType, uint64_t size,
                                const std::string &constructorName,
                                bool isStatic) {
  RuntimeClassInfo info;
  info.structType = structType;
  info.size = size;
  info.constructorName = constructorName;
  info.isStatic = isStatic;

  if (isStatic) {
    // Allocate static instance using QStaticRegistry
    info.staticInstancePtr =
        QStaticRegistry::Instance().GetOrCreateInstance(className, size);
    std::cout << "[DEBUG] QJitProgram: Registered static class '" << className
              << "' at " << info.staticInstancePtr << std::endl;
  }

  m_RegisteredClasses[className] = info;
}

void QJitProgram::RegisterMethod(const std::string &className,
                                 const std::string &methodName,
                                 const std::vector<QJParamType> &paramTypes) {
  auto it = m_RegisteredClasses.find(className);
  if (it == m_RegisteredClasses.end()) {
    std::cerr
        << "[WARNING] QJitProgram: RegisterMethod called for unknown class '"
        << className << "'" << std::endl;
    return;
  }

  MethodSignature sig;
  sig.paramTypes = paramTypes;
  sig.returnsVoid = true;
  it->second.methods[methodName] = sig;
}

void QJitProgram::RegisterMember(const std::string &className,
                                 const std::string &memberName, size_t offset,
                                 size_t size, int typeToken,
                                 const std::string &typeName) {
  auto it = m_RegisteredClasses.find(className);
  if (it == m_RegisteredClasses.end()) {
    std::cerr
        << "[WARNING] QJitProgram: RegisterMember called for unknown class '"
        << className << "'" << std::endl;
    return;
  }

  MemberInfo info;
  info.offset = offset;
  info.size = size;
  info.typeToken = typeToken;
  info.typeName = typeName;
  it->second.members[memberName] = info;
}

std::shared_ptr<QJClassInstance>
QJitProgram::CreateClassInstance(const std::string &className) {
  auto it = m_RegisteredClasses.find(className);
  if (it == m_RegisteredClasses.end()) {
    std::cerr << "[ERROR] QJitProgram: Class '" << className
              << "' not registered for instance creation" << std::endl;
    return nullptr;
  }

  const RuntimeClassInfo &info = it->second;

  // Allocate memory for the instance
  void *instancePtr = malloc(static_cast<size_t>(info.size));
  if (!instancePtr) {
    std::cerr << "[ERROR] QJitProgram: Failed to allocate memory for class '"
              << className << "'" << std::endl;
    return nullptr;
  }

  // Zero-initialize
  memset(instancePtr, 0, static_cast<size_t>(info.size));

  // Call constructor if available
  if (!info.constructorName.empty()) {
    uint64_t ctorAddr = GetFunctionAddress(info.constructorName);
    if (ctorAddr) {
      using CtorFn = void (*)(void *);
      CtorFn ctor = reinterpret_cast<CtorFn>(ctorAddr);
      ctor(instancePtr);
    }
  }

  // Create instance and register member info
  auto instance = std::make_shared<QJClassInstance>(className, instancePtr);

  // Copy member info to instance for runtime access
  for (const auto &member : info.members) {
    instance->RegisterMember(member.first, member.second);
  }

  return instance;
}

std::shared_ptr<QJClassInstance>
QJitProgram::GetStaticInstance(const std::string &className) {
  auto it = m_RegisteredClasses.find(className);
  if (it == m_RegisteredClasses.end()) {
    std::cerr << "[ERROR] QJitProgram: Class '" << className
              << "' not registered" << std::endl;
    return nullptr;
  }

  const RuntimeClassInfo &info = it->second;

  if (!info.isStatic) {
    std::cerr << "[ERROR] QJitProgram: Class '" << className
              << "' is not a static class. Use CreateClassInstance instead."
              << std::endl;
    return nullptr;
  }

  if (!info.staticInstancePtr) {
    std::cerr << "[ERROR] QJitProgram: Static class '" << className
              << "' has no allocated instance" << std::endl;
    return nullptr;
  }

  // Create a wrapper instance pointing to the static memory
  auto instance =
      std::make_shared<QJClassInstance>(className, info.staticInstancePtr);

  // Copy member info to instance for runtime access
  for (const auto &member : info.members) {
    instance->RegisterMember(member.first, member.second);
  }

  return instance;
}

// Pack a QJValue into a void* slot for passing through the wrapper
static void *PackArgToVoidPtr(const QJValue &val) {
  union {
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
    void *ptr;
    const char *cstr;
    uintptr_t raw;
  } u = {};

  switch (val.type) {
  case QJValue::Type::Int32:
    u.raw = 0;
    u.i32 = val.data.i32;
    break;
  case QJValue::Type::Int64:
    u.i64 = val.data.i64;
    break;
  case QJValue::Type::Float32:
    u.raw = 0;
    // Bitcast float to int32, then to pointer
    memcpy(&u.i32, &val.data.f32, sizeof(float));
    break;
  case QJValue::Type::Float64:
    memcpy(&u.i64, &val.data.f64, sizeof(double));
    break;
  case QJValue::Type::Bool:
    u.raw = val.data.b ? 1 : 0;
    break;
  case QJValue::Type::Ptr:
    u.ptr = val.data.ptr;
    break;
  case QJValue::Type::CStr:
    u.cstr = val.data.cstr;
    break;
  default:
    break;
  }

  return reinterpret_cast<void *>(u.raw);
}

void QJitProgram::CallMethodDynamic(uint64_t funcAddr, void *thisPtr,
                                    const std::vector<QJValue> &args,
                                    const MethodSignature &sig) {
  // Reserved for future use
}

QJValue QJitProgram::CallMethod(std::shared_ptr<QJClassInstance> instance,
                                const std::string &methodName,
                                const std::vector<QJValue> &args) {
  if (!instance || !instance->IsValid()) {
    std::cerr << "[ERROR] QJitProgram: Invalid instance for method call"
              << std::endl;
    return QJValue();
  }

  // Try to find the wrapper function first (generated at compile time)
  std::string wrapperName =
      instance->GetClassName() + "_" + methodName + "__wrap";
  uint64_t wrapperAddr = GetFunctionAddress(wrapperName);

  if (wrapperAddr) {
    // Use the wrapper for dynamic calling - no type dispatch needed!
    void *thisPtr = instance->GetInstancePtr();

    // Pack all arguments into a void** array
    std::vector<void *> packedArgs(args.size());
    for (size_t i = 0; i < args.size(); i++) {
      packedArgs[i] = PackArgToVoidPtr(args[i]);
    }

    // Call the wrapper: void(void* this, void** args)
    using WrapperFn = void (*)(void *, void **);
    WrapperFn wrapper = reinterpret_cast<WrapperFn>(wrapperAddr);

    try {
      wrapper(thisPtr, packedArgs.data());
    } catch (std::exception &e) {
      std::cerr << "[ERROR] QJitProgram: Exception: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "[ERROR] QJitProgram: Unknown exception" << std::endl;
    }

    return QJValue();
  }

  // Fallback: Try calling the method directly (for methods without wrappers)
  std::string fullMethodName = instance->GetClassName() + "_" + methodName;
  uint64_t funcAddr = GetFunctionAddress(fullMethodName);

  if (!funcAddr) {
    std::cerr << "[ERROR] QJitProgram: Method '" << fullMethodName
              << "' not found (no wrapper either)" << std::endl;
    return QJValue();
  }

  // Fallback to 0-arg call if no wrapper available
  if (args.empty()) {
    void *thisPtr = instance->GetInstancePtr();
    using Fn = void (*)(void *);
    reinterpret_cast<Fn>(funcAddr)(thisPtr);
  } else {
    std::cerr
        << "[ERROR] QJitProgram: No wrapper available for method with args"
        << std::endl;
  }

  return QJValue();
}

QMethodHandle
QJitProgram::GetMethodHandle(std::shared_ptr<QJClassInstance> instance,
                             const std::string &methodName) {
  if (!instance || !instance->IsValid()) {
    return QMethodHandle();
  }

  // Look up the wrapper function once
  std::string wrapperName =
      instance->GetClassName() + "_" + methodName + "__wrap";
  uint64_t wrapperAddr = GetFunctionAddress(wrapperName);

  if (wrapperAddr) {
    return QMethodHandle(instance, wrapperAddr);
  }

  std::cerr << "[ERROR] QJitProgram: No wrapper found for method '"
            << methodName << "'" << std::endl;
  return QMethodHandle();
}
