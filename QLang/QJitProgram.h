#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "QJClassInstance.h"
#include "QJValue.h"
#include "QMethodHandle.h"

namespace llvm {
class ExecutionEngine;
class Module;
class StructType;
class Type;
} // namespace llvm

// Method parameter type enum - matches QLang types
enum class QJParamType { Int32, Int64, Float32, Float64, Bool, String, Ptr };

// Stores information about a method's signature
struct MethodSignature {
  std::vector<QJParamType> paramTypes;
  QJParamType returnType =
      QJParamType::Int32; // Default, Void represented as Int32 with 0 return
  bool returnsVoid = true;
};

// Stores compiled class metadata for runtime instance creation
struct RuntimeClassInfo {
  llvm::StructType *structType = nullptr;
  uint64_t size = 0;
  std::string constructorName;
  std::unordered_map<std::string, MethodSignature> methods;
  std::unordered_map<std::string, MemberInfo>
      members;           // Member offset info for get/set
  bool isStatic = false; // True if this is a static class (singleton)
  void *staticInstancePtr = nullptr; // Pointer to static instance (if isStatic)
};

class QJitProgram {
public:
  QJitProgram(std::unique_ptr<llvm::Module> module);
  ~QJitProgram();

  static QJitProgram *Instance() { return s_Instance; }
  static void SetInstance(QJitProgram *instance) { s_Instance = instance; }

  void Run();

  // Get address of a JIT-compiled function by name
  uint64_t GetFunctionAddress(const std::string &funcName);

  // Register a class for runtime instance creation
  void RegisterClass(const std::string &className, llvm::StructType *structType,
                     uint64_t size, const std::string &constructorName,
                     bool isStatic = false);

  // Register a class member for runtime get/set access
  void RegisterMember(const std::string &className,
                      const std::string &memberName, size_t offset, size_t size,
                      int typeToken, const std::string &typeName = "");

  // Register a method signature for dynamic calling
  void RegisterMethod(const std::string &className,
                      const std::string &methodName,
                      const std::vector<QJParamType> &paramTypes);

  // Create an instance of a registered class
  std::shared_ptr<QJClassInstance>
  CreateClassInstance(const std::string &className);

  // Get the singleton instance of a static class
  std::shared_ptr<QJClassInstance>
  GetStaticInstance(const std::string &className);

  // Get class info for manual instance creation (wrapping pointers)
  const RuntimeClassInfo *GetClassInfo(const std::string &className) const {
    auto it = m_RegisteredClasses.find(className);
    if (it != m_RegisteredClasses.end()) {
      return &it->second;
    }
    return nullptr;
  }

  // ==== Method Calling Options (slowest to fastest) ====

  // 1. Dynamic call - most flexible, slowest (~100ns per call)
  QJValue CallMethod(std::shared_ptr<QJClassInstance> instance,
                     const std::string &methodName,
                     const std::vector<QJValue> &args = {});

  // 2. Cached handle - lookup once, fast calls (~30ns per call)
  QMethodHandle GetMethodHandle(std::shared_ptr<QJClassInstance> instance,
                                const std::string &methodName);

  // 3. Typed handle - zero overhead typed calls (~5ns per call)
  template <typename... Args>
  QTypedMethodHandle<Args...>
  GetTypedMethodHandle(std::shared_ptr<QJClassInstance> instance,
                       const std::string &methodName) {
    if (!instance || !instance->IsValid()) {
      return QTypedMethodHandle<Args...>();
    }
    std::string fullName = instance->GetClassName() + "_" + methodName;
    uint64_t funcAddr = GetFunctionAddress(fullName);
    if (funcAddr) {
      return QTypedMethodHandle<Args...>(instance->GetInstancePtr(), funcAddr);
    }
    return QTypedMethodHandle<Args...>();
  }

  // 4. Raw function pointer - absolute minimum overhead (~1-2ns per call)
  template <typename... Args>
  QRawMethodPtr<Args...> GetRawMethodPtr(const std::string &className,
                                         const std::string &methodName) {
    std::string fullName = className + "_" + methodName;
    uint64_t funcAddr = GetFunctionAddress(fullName);
    return QRawMethodPtr<Args...>(funcAddr);
  }

private:
  // Internal helper for dynamic function calling
  void CallMethodDynamic(uint64_t funcAddr, void *thisPtr,
                         const std::vector<QJValue> &args,
                         const MethodSignature &sig);

  llvm::ExecutionEngine *m_Engine = nullptr;
  std::unordered_map<std::string, RuntimeClassInfo> m_RegisteredClasses;
  static QJitProgram *s_Instance;
};
