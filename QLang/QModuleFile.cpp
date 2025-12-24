#include "QModuleFile.h"
#include <iostream>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

void QModuleFile::WriteString(std::ostream &os, const std::string &str) {
  uint32_t len = static_cast<uint32_t>(str.size());
  os.write(reinterpret_cast<const char *>(&len), sizeof(len));
  os.write(str.data(), len);
}

std::string QModuleFile::ReadString(std::istream &is) {
  uint32_t len = 0;
  is.read(reinterpret_cast<char *>(&len), sizeof(len));
  if (len > 0 && len < 1000000) { // Sanity check
    std::string str(len, '\0');
    is.read(&str[0], len);
    return str;
  }
  return "";
}

void QModuleFile::WriteInt32(std::ostream &os, int32_t value) {
  os.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

int32_t QModuleFile::ReadInt32(std::istream &is) {
  int32_t value = 0;
  is.read(reinterpret_cast<char *>(&value), sizeof(value));
  return value;
}

void QModuleFile::WriteUInt32(std::ostream &os, uint32_t value) {
  os.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

uint32_t QModuleFile::ReadUInt32(std::istream &is) {
  uint32_t value = 0;
  is.read(reinterpret_cast<char *>(&value), sizeof(value));
  return value;
}

bool QModuleFile::SaveModule(const std::string &moduleName,
                             const std::string &filePath, llvm::Module *module,
                             const std::vector<ModuleClassInfo> &classes) {
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    m_ErrorMessage = "Failed to open file for writing: " + filePath;
    return false;
  }

  std::cout << "[INFO] QModuleFile: Saving module '" << moduleName << "' to "
            << filePath << std::endl;

  // Write header
  WriteUInt32(file, MAGIC);
  WriteUInt32(file, VERSION);
  WriteString(file, moduleName);

  // Write class metadata
  WriteUInt32(file, static_cast<uint32_t>(classes.size()));
  for (const auto &cls : classes) {
    WriteString(file, cls.className);

    // Write members
    WriteUInt32(file, static_cast<uint32_t>(cls.memberNames.size()));
    for (size_t i = 0; i < cls.memberNames.size(); ++i) {
      WriteString(file, cls.memberNames[i]);
      WriteInt32(file, cls.memberTypeTokens[i]);
      WriteString(file, cls.memberTypeNames[i]);
    }

    // Write method names
    WriteUInt32(file, static_cast<uint32_t>(cls.methodNames.size()));
    for (const auto &methodName : cls.methodNames) {
      WriteString(file, methodName);
      // Write return type
      std::string retType = "";
      auto it = cls.methodReturnTypes.find(methodName);
      if (it != cls.methodReturnTypes.end()) {
        retType = it->second;
      }
      WriteString(file, retType);
    }

    // Write static flag
    WriteInt32(file, cls.isStatic ? 1 : 0);

    std::cout << "[DEBUG] QModuleFile: Wrote class '" << cls.className
              << "' with " << cls.memberNames.size() << " members and "
              << cls.methodNames.size() << " methods"
              << (cls.isStatic ? " (STATIC)" : "") << std::endl;
  }

  // Write LLVM bitcode
  std::cout << "[DEBUG] QModuleFile: Writing bitcode for module..."
            << std::endl;
#if QLANG_DEBUG
  module->print(llvm::outs(), nullptr);
#endif

  std::string bitcodeStr;
  llvm::raw_string_ostream bitcodeOS(bitcodeStr);
  llvm::WriteBitcodeToFile(*module, bitcodeOS);
  bitcodeOS.flush();

  WriteUInt32(file, static_cast<uint32_t>(bitcodeStr.size()));
  file.write(bitcodeStr.data(), bitcodeStr.size());

  std::cout << "[DEBUG] QModuleFile: Wrote " << bitcodeStr.size()
            << " bytes of bitcode" << std::endl;

  file.close();
  std::cout << "[INFO] QModuleFile: Successfully saved module to " << filePath
            << std::endl;
  return true;
}

bool QModuleFile::LoadModule(const std::string &filePath,
                             llvm::LLVMContext &context,
                             std::unique_ptr<llvm::Module> &outModule,
                             std::vector<ModuleClassInfo> &outClasses) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file) {
    m_ErrorMessage = "Failed to open file for reading: " + filePath;
    return false;
  }

  std::cout << "[INFO] QModuleFile: Loading module from " << filePath
            << std::endl;

  // Read and verify header
  uint32_t magic = ReadUInt32(file);
  if (magic != MAGIC) {
    m_ErrorMessage = "Invalid module file magic number";
    return false;
  }

  uint32_t version = ReadUInt32(file);
  if (version != VERSION) {
    m_ErrorMessage =
        "Unsupported module file version: " + std::to_string(version);
    return false;
  }

  std::string moduleName = ReadString(file);
  std::cout << "[DEBUG] QModuleFile: Loading module '" << moduleName << "'"
            << std::endl;

  // Read class metadata
  uint32_t classCount = ReadUInt32(file);
  outClasses.clear();
  outClasses.reserve(classCount);

  for (uint32_t c = 0; c < classCount; ++c) {
    ModuleClassInfo cls;
    cls.className = ReadString(file);

    // Read members
    uint32_t memberCount = ReadUInt32(file);
    cls.memberNames.reserve(memberCount);
    cls.memberTypeTokens.reserve(memberCount);
    cls.memberTypeNames.reserve(memberCount);

    for (uint32_t m = 0; m < memberCount; ++m) {
      cls.memberNames.push_back(ReadString(file));
      cls.memberTypeTokens.push_back(ReadInt32(file));
      cls.memberTypeNames.push_back(ReadString(file));
    }

    // Read method names
    uint32_t methodCount = ReadUInt32(file);
    cls.methodNames.reserve(methodCount);
    for (uint32_t m = 0; m < methodCount; ++m) {
      std::string methodName = ReadString(file);
      cls.methodNames.push_back(methodName);
      std::string retType = ReadString(file);
      if (!retType.empty()) {
        cls.methodReturnTypes[methodName] = retType;
      }
    }

    // Read static flag
    cls.isStatic = ReadInt32(file) != 0;

    std::cout << "[DEBUG] QModuleFile: Loaded class '" << cls.className
              << "' with " << cls.memberNames.size() << " members and "
              << cls.methodNames.size() << " methods"
              << (cls.isStatic ? " (STATIC)" : "") << std::endl;

    outClasses.push_back(cls);
  }

  // Read LLVM bitcode
  uint32_t bitcodeSize = ReadUInt32(file);
  std::vector<char> bitcodeData(bitcodeSize);
  file.read(bitcodeData.data(), bitcodeSize);

  std::cout << "[DEBUG] QModuleFile: Read " << bitcodeSize
            << " bytes of bitcode" << std::endl;

  // Parse bitcode
  llvm::StringRef bitcodeRef(bitcodeData.data(), bitcodeSize);
  auto bufferOrErr = llvm::MemoryBuffer::getMemBuffer(bitcodeRef, "", false);
  if (!bufferOrErr) {
    m_ErrorMessage = "Failed to create memory buffer for bitcode";
    return false;
  }

  auto moduleOrErr =
      llvm::parseBitcodeFile(bufferOrErr->getMemBufferRef(), context);
  if (!moduleOrErr) {
    std::string errStr;
    llvm::raw_string_ostream errOS(errStr);
    errOS << moduleOrErr.takeError();
    m_ErrorMessage = "Failed to parse bitcode: " + errStr;
    return false;
  }

  outModule = std::move(moduleOrErr.get());

  std::cout << "[INFO] QModuleFile: Successfully loaded module from "
            << filePath << std::endl;
  return true;
}
