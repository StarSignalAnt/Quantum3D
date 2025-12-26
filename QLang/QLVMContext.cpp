#include "QLVMContext.h"
#include "QLVM.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/DynamicLibrary.h>

// ============================================================================
// Built-in Native Functions for QLang
// ============================================================================

// Native printf for QLang scripts
extern "C" void LV_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}

// String concatenation for QLang
extern "C" char *LV_str_concat(const char *s1, const char *s2) {
  if (!s1)
    s1 = "";
  if (!s2)
    s2 = "";
  char *result = (char *)malloc(strlen(s1) + strlen(s2) + 1);
  strcpy(result, s1);
  strcat(result, s2);
  return result;
}

// ToString helper functions for runtime conversion
extern "C" char *LV_int32_to_string(int32_t value) {
  char *result = (char *)malloc(16);
  snprintf(result, 16, "%d", value);
  return result;
}

extern "C" char *LV_int64_to_string(int64_t value) {
  char *result = (char *)malloc(24);
  snprintf(result, 24, "%lld", value);
  return result;
}

extern "C" char *LV_float32_to_string(float value) {
  char *result = (char *)malloc(24);
  snprintf(result, 24, "%g", value);
  return result;
}

extern "C" char *LV_float64_to_string(double value) {
  char *result = (char *)malloc(32);
  snprintf(result, 32, "%g", value);
  return result;
}

extern "C" char *LV_bool_to_string(int8_t value) {
  return value ? _strdup("true") : _strdup("false");
}

// String-to-number conversion functions (for ToInt/ToFloat methods)
extern "C" int32_t LV_string_to_int32(const char *str) {
  if (!str)
    return 0;
  return (int32_t)atoi(str);
}

extern "C" int64_t LV_string_to_int64(const char *str) {
  if (!str)
    return 0;
  return (int64_t)atoll(str);
}

extern "C" float LV_string_to_float32(const char *str) {
  if (!str)
    return 0.0f;
  return (float)atof(str);
}

extern "C" double LV_string_to_float64(const char *str) {
  if (!str)
    return 0.0;
  return atof(str);
}

// ============================================================================
// QLVMContext Implementation
// ============================================================================

QLVMContext::QLVMContext() {
  // Auto-register all built-in functions
  RegisterBuiltinFunctions();
}

QLVMContext::~QLVMContext() {}

void QLVMContext::RegisterBuiltinFunctions() {
  auto &context = QLVM::GetContext();

  // qprintf - variadic printf for QLang
  auto *qprintfType =
      llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                              {llvm::PointerType::getUnqual(context)}, true);
  AddFunc("qprintf", (void *)LV_printf, qprintfType);

  // string_concat - concatenate two strings
  auto *strConcatType =
      llvm::FunctionType::get(llvm::PointerType::getUnqual(context),
                              {llvm::PointerType::getUnqual(context),
                               llvm::PointerType::getUnqual(context)},
                              false);
  AddFunc("string_concat", (void *)LV_str_concat, strConcatType);

  // ToString helper functions for runtime numeric/bool to string conversion
  auto *int32ToStrType =
      llvm::FunctionType::get(llvm::PointerType::getUnqual(context),
                              {llvm::Type::getInt32Ty(context)}, false);
  AddFunc("__int32_to_string", (void *)LV_int32_to_string, int32ToStrType);

  auto *int64ToStrType =
      llvm::FunctionType::get(llvm::PointerType::getUnqual(context),
                              {llvm::Type::getInt64Ty(context)}, false);
  AddFunc("__int64_to_string", (void *)LV_int64_to_string, int64ToStrType);

  auto *float32ToStrType =
      llvm::FunctionType::get(llvm::PointerType::getUnqual(context),
                              {llvm::Type::getFloatTy(context)}, false);
  AddFunc("__float32_to_string", (void *)LV_float32_to_string,
          float32ToStrType);

  auto *float64ToStrType =
      llvm::FunctionType::get(llvm::PointerType::getUnqual(context),
                              {llvm::Type::getDoubleTy(context)}, false);
  AddFunc("__float64_to_string", (void *)LV_float64_to_string,
          float64ToStrType);

  auto *boolToStrType =
      llvm::FunctionType::get(llvm::PointerType::getUnqual(context),
                              {llvm::Type::getInt1Ty(context)}, false);
  AddFunc("__bool_to_string", (void *)LV_bool_to_string, boolToStrType);

  // String-to-number conversion functions (ToInt/ToFloat methods)
  auto *strToInt32Type =
      llvm::FunctionType::get(llvm::Type::getInt32Ty(context),
                              {llvm::PointerType::getUnqual(context)}, false);
  AddFunc("__string_to_int32", (void *)LV_string_to_int32, strToInt32Type);

  auto *strToInt64Type =
      llvm::FunctionType::get(llvm::Type::getInt64Ty(context),
                              {llvm::PointerType::getUnqual(context)}, false);
  AddFunc("__string_to_int64", (void *)LV_string_to_int64, strToInt64Type);

  auto *strToFloat32Type =
      llvm::FunctionType::get(llvm::Type::getFloatTy(context),
                              {llvm::PointerType::getUnqual(context)}, false);
  AddFunc("__string_to_float32", (void *)LV_string_to_float32,
          strToFloat32Type);

  auto *strToFloat64Type =
      llvm::FunctionType::get(llvm::Type::getDoubleTy(context),
                              {llvm::PointerType::getUnqual(context)}, false);
  AddFunc("__string_to_float64", (void *)LV_string_to_float64,
          strToFloat64Type);
}

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
