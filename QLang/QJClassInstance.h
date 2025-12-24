#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

// Stores runtime information about a class member for get/set access
struct MemberInfo {
  size_t offset = 0;    // Byte offset within the struct
  size_t size = 0;      // Size in bytes
  int typeToken = 0;    // Token type for type checking
  std::string typeName; // Type name for class members
};

// Holds a runtime instance of a QLang class created by QJitRunner
// Provides access to member variables and methods
class QJClassInstance {
public:
  QJClassInstance(const std::string &className, void *instancePtr)
      : m_ClassName(className), m_InstancePtr(instancePtr) {}

  ~QJClassInstance() = default;

  // Get the class name
  const std::string &GetClassName() const { return m_ClassName; }

  // Get the raw pointer to the instance memory
  void *GetInstancePtr() const { return m_InstancePtr; }

  // Check if the instance is valid
  bool IsValid() const { return m_InstancePtr != nullptr; }

  // Register member info for runtime access (called by QJitProgram)
  void RegisterMember(const std::string &name, const MemberInfo &info) {
    m_Members[name] = info;
  }

  // Check if a member exists
  bool HasMember(const std::string &name) const {
    return m_Members.find(name) != m_Members.end();
  }

  // Get a member value by name
  // Usage: int32_t age = instance->GetMember<int32_t>("age");
  template <typename T> T GetMember(const std::string &name) const {
    auto it = m_Members.find(name);
    if (it == m_Members.end() || !m_InstancePtr) {
      return T{}; // Return default if member not found
    }

    const MemberInfo &info = it->second;
    char *ptr = static_cast<char *>(m_InstancePtr) + info.offset;

    T result{};
    memcpy(&result, ptr, sizeof(T));
    return result;
  }

  // Get string member (special case - stored as char*)
  std::string GetStringMember(const std::string &name) const {
    auto it = m_Members.find(name);
    if (it == m_Members.end() || !m_InstancePtr) {
      return "";
    }

    const MemberInfo &info = it->second;
    char *ptr = static_cast<char *>(m_InstancePtr) + info.offset;

    // String is stored as char* pointer
    const char *strPtr = *reinterpret_cast<const char **>(ptr);
    return strPtr ? std::string(strPtr) : "";
  }

  // Set a member value by name
  // Usage: instance->SetMember<int32_t>("age", 30);
  template <typename T> void SetMember(const std::string &name, T value) {
    auto it = m_Members.find(name);
    if (it == m_Members.end() || !m_InstancePtr) {
      return;
    }

    const MemberInfo &info = it->second;
    char *ptr = static_cast<char *>(m_InstancePtr) + info.offset;

    memcpy(ptr, &value, sizeof(T));
  }

  // Set string member (special case)
  void SetStringMember(const std::string &name, const char *value) {
    auto it = m_Members.find(name);
    if (it == m_Members.end() || !m_InstancePtr) {
      return;
    }

    const MemberInfo &info = it->second;
    char *ptr = static_cast<char *>(m_InstancePtr) + info.offset;

    // Store the pointer directly
    *reinterpret_cast<const char **>(ptr) = value;
  }

  // Get the members map (for debugging)
  const std::unordered_map<std::string, MemberInfo> &GetMembers() const {
    return m_Members;
  }

private:
  std::string m_ClassName;
  void *m_InstancePtr;
  std::unordered_map<std::string, MemberInfo> m_Members;
};
