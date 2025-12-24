#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>


// QStaticRegistry - stores static class instances that persist across modules
// This is a global singleton that holds memory for static classes
class QStaticRegistry {
public:
  static QStaticRegistry &Instance() {
    static QStaticRegistry instance;
    return instance;
  }

  // Allocate or get an existing static class instance
  void *GetOrCreateInstance(const std::string &className, uint64_t size) {
    auto it = m_Instances.find(className);
    if (it != m_Instances.end()) {
      std::cout << "[DEBUG] QStaticRegistry: Returning existing instance of '"
                << className << "' at " << it->second << std::endl;
      return it->second;
    }

    // Allocate new instance
    void *ptr = malloc(static_cast<size_t>(size));
    if (ptr) {
      memset(ptr, 0, static_cast<size_t>(size));
      m_Instances[className] = ptr;
      std::cout << "[DEBUG] QStaticRegistry: Created new static instance of '"
                << className << "' at " << ptr << std::endl;
    }
    return ptr;
  }

  // Get an existing instance (returns nullptr if not found)
  void *GetInstance(const std::string &className) const {
    auto it = m_Instances.find(className);
    if (it != m_Instances.end()) {
      return it->second;
    }
    return nullptr;
  }

  // Check if a static class instance exists
  bool HasInstance(const std::string &className) const {
    return m_Instances.find(className) != m_Instances.end();
  }

  // Get all registered static class names
  std::vector<std::string> GetStaticClassNames() const {
    std::vector<std::string> names;
    for (const auto &pair : m_Instances) {
      names.push_back(pair.first);
    }
    return names;
  }

  // Clear all static instances (for testing/cleanup)
  void Clear() {
    for (auto &pair : m_Instances) {
      free(pair.second);
    }
    m_Instances.clear();
  }

private:
  QStaticRegistry() = default;
  ~QStaticRegistry() {
    // Don't free on destruction - let the OS clean up
    // This prevents issues with static destruction order
  }

  // Disable copy/move
  QStaticRegistry(const QStaticRegistry &) = delete;
  QStaticRegistry &operator=(const QStaticRegistry &) = delete;

  std::unordered_map<std::string, void *> m_Instances;
};
