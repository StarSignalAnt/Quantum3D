#include "QJClassInstance.h"
#include "QJitProgram.h"
#include <iostream>

QJClassInstance::QJClassInstance(const std::string &className,
                                 void *instancePtr)
    : m_ClassName(className), m_InstancePtr(instancePtr) {

  // Auto-populate members from the running program registry if available
  if (QJitProgram::Instance()) {
    auto classInfo = QJitProgram::Instance()->GetClassInfo(className);
    if (classInfo) {
      m_Members = classInfo->members;
    } else {
      // Warning: Class info not found, instance might not support member access
      std::cerr << "[WARNING] QJClassInstance: Class '" << className
                << "' not found in registry (ptr=" << instancePtr << ")"
                << std::endl;
    }
  }
}

template <typename T>
T QJClassInstance::GetMember(const std::string &name) const {
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

// Explicit template instantiations for common types to allow linking
template int32_t QJClassInstance::GetMember<int32_t>(const std::string &) const;
template int64_t QJClassInstance::GetMember<int64_t>(const std::string &) const;
template float QJClassInstance::GetMember<float>(const std::string &) const;
template double QJClassInstance::GetMember<double>(const std::string &) const;
template bool QJClassInstance::GetMember<bool>(const std::string &) const;
template void *QJClassInstance::GetMember<void *>(const std::string &) const;

std::string QJClassInstance::GetStringMember(const std::string &name) const {
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

template <typename T>
void QJClassInstance::SetMember(const std::string &name, T value) {
  auto it = m_Members.find(name);
  if (it == m_Members.end() || !m_InstancePtr) {
    return;
  }

  const MemberInfo &info = it->second;
  char *ptr = static_cast<char *>(m_InstancePtr) + info.offset;

  memcpy(ptr, &value, sizeof(T));
}

// Explicit template instantiations for set
template void QJClassInstance::SetMember<int32_t>(const std::string &, int32_t);
template void QJClassInstance::SetMember<int64_t>(const std::string &, int64_t);
template void QJClassInstance::SetMember<float>(const std::string &, float);
template void QJClassInstance::SetMember<double>(const std::string &, double);
template void QJClassInstance::SetMember<bool>(const std::string &, bool);
template void QJClassInstance::SetMember<void *>(const std::string &, void *);

void QJClassInstance::SetStringMember(const std::string &name,
                                      const char *value) {
  auto it = m_Members.find(name);
  if (it == m_Members.end() || !m_InstancePtr) {
    return;
  }

  const MemberInfo &info = it->second;
  char *ptr = static_cast<char *>(m_InstancePtr) + info.offset;

  // Store the pointer directly
  *reinterpret_cast<const char **>(ptr) = value;
}

void *QJClassInstance::GetPtrMember(const std::string &name) const {
  auto it = m_Members.find(name);
  if (it == m_Members.end() || !m_InstancePtr) {
    return nullptr;
  }

  const MemberInfo &info = it->second;
  char *ptr = static_cast<char *>(m_InstancePtr) + info.offset;

  // Return the stored pointer
  return *reinterpret_cast<void **>(ptr);
}

void QJClassInstance::SetPtrMember(const std::string &name, void *value) {
  auto it = m_Members.find(name);
  if (it == m_Members.end() || !m_InstancePtr) {
    return;
  }

  const MemberInfo &info = it->second;
  char *ptr = static_cast<char *>(m_InstancePtr) + info.offset;

  // Store the pointer
  *reinterpret_cast<void **>(ptr) = value;
}
