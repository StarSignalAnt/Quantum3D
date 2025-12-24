#pragma once

#include <cstdint>

// Simple value type for passing arguments to QLang methods from C++
// Using a struct instead of std::variant for ABI compatibility
struct QJValue {
  enum class Type {
    Null,
    Int32,
    Int64,
    Float32,
    Float64,
    Bool,
    Ptr,
    CStr // C-style string (const char*)
  };

  Type type = Type::Null;

  union {
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
    bool b;
    void *ptr;
    const char *cstr;
  } data = {};

  // Constructors
  QJValue() : type(Type::Null) { data.i64 = 0; }
  QJValue(int32_t v) : type(Type::Int32) { data.i32 = v; }
  QJValue(int64_t v) : type(Type::Int64) { data.i64 = v; }
  QJValue(float v) : type(Type::Float32) { data.f32 = v; }
  QJValue(double v) : type(Type::Float64) { data.f64 = v; }
  QJValue(bool v) : type(Type::Bool) { data.b = v; }
  QJValue(void *v) : type(Type::Ptr) { data.ptr = v; }
  QJValue(const char *v) : type(Type::CStr) { data.cstr = v; }

  // Type checkers
  bool IsNull() const { return type == Type::Null; }
  bool IsInt32() const { return type == Type::Int32; }
  bool IsInt64() const { return type == Type::Int64; }
  bool IsFloat32() const { return type == Type::Float32; }
  bool IsFloat64() const { return type == Type::Float64; }
  bool IsBool() const { return type == Type::Bool; }
  bool IsPtr() const { return type == Type::Ptr; }
  bool IsCStr() const { return type == Type::CStr; }

  // Getters
  int32_t GetInt32() const { return data.i32; }
  int64_t GetInt64() const { return data.i64; }
  float GetFloat32() const { return data.f32; }
  double GetFloat64() const { return data.f64; }
  bool GetBool() const { return data.b; }
  void *GetPtr() const { return data.ptr; }
  const char *GetCStr() const { return data.cstr; }
};

// Helper to get type name for debugging
inline const char *GetQJValueTypeName(const QJValue &val) {
  switch (val.type) {
  case QJValue::Type::Null:
    return "null";
  case QJValue::Type::Int32:
    return "int32";
  case QJValue::Type::Int64:
    return "int64";
  case QJValue::Type::Float32:
    return "float32";
  case QJValue::Type::Float64:
    return "float64";
  case QJValue::Type::Bool:
    return "bool";
  case QJValue::Type::Ptr:
    return "ptr";
  case QJValue::Type::CStr:
    return "cstr";
  default:
    return "unknown";
  }
}
