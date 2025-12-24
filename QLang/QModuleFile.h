#pragma once
#include <fstream>
#include <llvm/IR/Module.h>
#include <memory>
#include <string>
#include <vector>

namespace llvm {
class MemoryBuffer;
}

// Metadata about a class stored in a module
struct ModuleClassInfo {
  std::string className;
  std::vector<std::string> memberNames;
  std::vector<int> memberTypeTokens;
  std::vector<std::string> memberTypeNames;
  std::vector<std::string> methodNames;
  std::unordered_map<std::string, std::string> methodReturnTypes;
};

// Handles reading/writing compiled QLang modules (.qm files)
class QModuleFile {
public:
  QModuleFile() = default;

  // Save a module to a binary file
  bool SaveModule(const std::string &moduleName, const std::string &filePath,
                  llvm::Module *module,
                  const std::vector<ModuleClassInfo> &classes);

  // Load a module from a binary file
  bool LoadModule(const std::string &filePath, llvm::LLVMContext &context,
                  std::unique_ptr<llvm::Module> &outModule,
                  std::vector<ModuleClassInfo> &outClasses);

  // Get the last error message
  const std::string &GetError() const { return m_ErrorMessage; }

private:
  std::string m_ErrorMessage;

  // Magic number and version for file format
  static constexpr uint32_t MAGIC = 0x514D4F44; // "QMOD"
  static constexpr uint32_t VERSION = 1;

  void WriteString(std::ostream &os, const std::string &str);
  std::string ReadString(std::istream &is);
  void WriteInt32(std::ostream &os, int32_t value);
  int32_t ReadInt32(std::istream &is);
  void WriteUInt32(std::ostream &os, uint32_t value);
  uint32_t ReadUInt32(std::istream &is);
};
