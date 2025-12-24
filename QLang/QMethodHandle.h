#pragma once

#include "QJClassInstance.h"
#include "QJValue.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// Pack a QJValue into a void* slot - inline for speed
inline void *PackArgFast(const QJValue &val) {
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

// ============================================================================
// QMethodHandle - Cached method handle with pre-allocated buffer
// Eliminates lookup overhead and vector allocation
// ============================================================================
class QMethodHandle {
public:
  static constexpr size_t MAX_ARGS = 8;

  QMethodHandle() = default;

  QMethodHandle(std::shared_ptr<QJClassInstance> instance, uint64_t wrapperAddr)
      : m_Instance(instance), m_WrapperAddr(wrapperAddr) {}

  bool IsValid() const {
    return m_Instance && m_Instance->IsValid() && m_WrapperAddr != 0;
  }

  // Call with QJValue vector - uses pre-allocated buffer
  void Call(const std::vector<QJValue> &args) const {
    if (!IsValid())
      return;

    // Use stack-allocated buffer instead of vector
    void *packedArgs[MAX_ARGS] = {};
    size_t count = args.size() < MAX_ARGS ? args.size() : MAX_ARGS;

    for (size_t i = 0; i < count; i++) {
      packedArgs[i] = PackArgFast(args[i]);
    }

    using WrapperFn = void (*)(void *, void **);
    auto wrapper = reinterpret_cast<WrapperFn>(m_WrapperAddr);
    wrapper(m_Instance->GetInstancePtr(), packedArgs);
  }

private:
  std::shared_ptr<QJClassInstance> m_Instance;
  uint64_t m_WrapperAddr = 0;
};

// ============================================================================
// QTypedMethodHandle - Zero-overhead typed method handle
// Stores the actual function pointer for direct calls
// ============================================================================
template <typename... Args> class QTypedMethodHandle {
public:
  using FnType = void (*)(void *, Args...);

  QTypedMethodHandle() = default;

  QTypedMethodHandle(void *thisPtr, uint64_t funcAddr)
      : m_ThisPtr(thisPtr), m_FuncPtr(reinterpret_cast<FnType>(funcAddr)) {}

  bool IsValid() const { return m_ThisPtr && m_FuncPtr; }

  // Direct call - zero overhead beyond the actual function call!
  void operator()(Args... args) const {
    if (m_FuncPtr) {
      m_FuncPtr(m_ThisPtr, args...);
    }
  }

  // Explicit call method
  void Call(Args... args) const {
    if (m_FuncPtr) {
      m_FuncPtr(m_ThisPtr, args...);
    }
  }

private:
  void *m_ThisPtr = nullptr;
  FnType m_FuncPtr = nullptr;
};

// ============================================================================
// QRawMethodPtr - Absolute minimum overhead - just a function pointer
// For maximum performance critical paths
// ============================================================================
template <typename... Args> class QRawMethodPtr {
public:
  using FnType = void (*)(void *, Args...);

  QRawMethodPtr() = default;
  QRawMethodPtr(uint64_t addr) : m_FuncPtr(reinterpret_cast<FnType>(addr)) {}

  bool IsValid() const { return m_FuncPtr != nullptr; }

  // Call with explicit this pointer - absolute minimum overhead
  void operator()(void *thisPtr, Args... args) const {
    m_FuncPtr(thisPtr, args...);
  }

private:
  FnType m_FuncPtr = nullptr;
};
