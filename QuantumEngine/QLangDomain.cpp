#include "QLangDomain.h"
#include <iostream>
#include <memory>
#include <variant>
#include <vector>

#include "GraphNode.h"
#include "Parser.h"
#include "QClassInstance.h"
#include "QContext.h"
#include "QError.h"
#include "QRunner.h"
#include "Tokenizer.h"
#include <filesystem>

// Global Funcs

// Native printf function for QLang
QValue func_printf(QContext *ctx, const std::vector<QValue> &args) {
  std::cout << "[OUTPUT] ";
  for (size_t i = 0; i < args.size(); i++) {
    std::cout << ValueToString(args[i]);
    if (i < args.size() - 1)
      std::cout << " ";
  }
  std::cout << std::endl;
  return std::monostate{}; // Returns null
}

QValue node_turn(QContext *ctx, const std::vector<QValue> &args) {
  // Validate arguments
  if (args.size() < 2) {
    std::cerr
        << "[ERROR] node_turn requires 2 arguments: cptr node, Vec3 rotation"
        << std::endl;
    return std::monostate{};
  }

  // Extract node pointer from arg 0 (cptr)
  if (!std::holds_alternative<void *>(args[0])) {
    std::cerr << "[ERROR] node_turn: arg 0 must be a cptr (node pointer)"
              << std::endl;
    return std::monostate{};
  }
  void *nodePtr = std::get<void *>(args[0]);
  Quantum::GraphNode *node = static_cast<Quantum::GraphNode *>(nodePtr);

  if (!node) {
    std::cerr << "[ERROR] node_turn: node pointer is null" << std::endl;
    return std::monostate{};
  }

  // Extract Vec3 class instance from arg 1
  if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(args[1])) {
    std::cerr << "[ERROR] node_turn: arg 1 must be a Vec3 class instance"
              << std::endl;
    return std::monostate{};
  }
  auto vec3Instance = std::get<std::shared_ptr<QClassInstance>>(args[1]);

  // Get x, y, z from the Vec3 instance
  float x = 0.0f, y = 0.0f, z = 0.0f;

  QInstanceValue xVal = vec3Instance->GetMember("X");
  QInstanceValue yVal = vec3Instance->GetMember("Y");
  QInstanceValue zVal = vec3Instance->GetMember("Z");

  // Extract float values (handle both float and int types)
  if (std::holds_alternative<float>(xVal))
    x = std::get<float>(xVal);
  else if (std::holds_alternative<int32_t>(xVal))
    x = static_cast<float>(std::get<int32_t>(xVal));
  else if (std::holds_alternative<double>(xVal))
    x = static_cast<float>(std::get<double>(xVal));

  if (std::holds_alternative<float>(yVal))
    y = std::get<float>(yVal);
  else if (std::holds_alternative<int32_t>(yVal))
    y = static_cast<float>(std::get<int32_t>(yVal));
  else if (std::holds_alternative<double>(yVal))
    y = static_cast<float>(std::get<double>(yVal));

  if (std::holds_alternative<float>(zVal))
    z = std::get<float>(zVal);
  else if (std::holds_alternative<int32_t>(zVal))
    z = static_cast<float>(std::get<int32_t>(zVal));
  else if (std::holds_alternative<double>(zVal))
    z = static_cast<float>(std::get<double>(zVal));

  // Apply rotation (Euler angles in degrees)
  // glm::vec3 currentRotation = node->GetRotationEuler();
  node->Turn(glm::vec3(x, y, z));

#if QLANG_DEBUG
  std::cout << "[DEBUG] node_turn: rotated node by (" << x << ", " << y << ", "
            << z << ")" << std::endl;
#endif

  return std::monostate{};
}

QValue node_setPosition(QContext *ctx, const std::vector<QValue> &args) {
  // Validate arguments
  if (args.size() < 2) {
    std::cerr
        << "[ERROR] node_turn requires 2 arguments: cptr node, Vec3 rotation"
        << std::endl;
    return std::monostate{};
  }

  // Extract node pointer from arg 0 (cptr)
  if (!std::holds_alternative<void *>(args[0])) {
    std::cerr << "[ERROR] node_turn: arg 0 must be a cptr (node pointer)"
              << std::endl;
    return std::monostate{};
  }
  void *nodePtr = std::get<void *>(args[0]);
  Quantum::GraphNode *node = static_cast<Quantum::GraphNode *>(nodePtr);

  if (!node) {
    std::cerr << "[ERROR] node_turn: node pointer is null" << std::endl;
    return std::monostate{};
  }

  // Extract Vec3 class instance from arg 1
  if (!std::holds_alternative<std::shared_ptr<QClassInstance>>(args[1])) {
    std::cerr << "[ERROR] node_turn: arg 1 must be a Vec3 class instance"
              << std::endl;
    return std::monostate{};
  }
  auto vec3Instance = std::get<std::shared_ptr<QClassInstance>>(args[1]);

  // Get x, y, z from the Vec3 instance
  float x = 0.0f, y = 0.0f, z = 0.0f;

  QInstanceValue xVal = vec3Instance->GetMember("X");
  QInstanceValue yVal = vec3Instance->GetMember("Y");
  QInstanceValue zVal = vec3Instance->GetMember("Z");

  // Extract float values (handle both float and int types)
  if (std::holds_alternative<float>(xVal))
    x = std::get<float>(xVal);
  else if (std::holds_alternative<int32_t>(xVal))
    x = static_cast<float>(std::get<int32_t>(xVal));
  else if (std::holds_alternative<double>(xVal))
    x = static_cast<float>(std::get<double>(xVal));

  if (std::holds_alternative<float>(yVal))
    y = std::get<float>(yVal);
  else if (std::holds_alternative<int32_t>(yVal))
    y = static_cast<float>(std::get<int32_t>(yVal));
  else if (std::holds_alternative<double>(yVal))
    y = static_cast<float>(std::get<double>(yVal));

  if (std::holds_alternative<float>(zVal))
    z = std::get<float>(zVal);
  else if (std::holds_alternative<int32_t>(zVal))
    z = static_cast<float>(std::get<int32_t>(zVal));
  else if (std::holds_alternative<double>(zVal))
    z = static_cast<float>(std::get<double>(zVal));

  // Apply rotation (Euler angles in degrees)
  // glm::vec3 currentRotation = node->GetRotationEuler();
  node->SetLocalPosition(glm::vec3(x, y, z));

#if QLANG_DEBUG
  std::cout << "[DEBUG] node_turn: rotated node by (" << x << ", " << y << ", "
            << z << ")" << std::endl;
#endif

  return std::monostate{};
}

// Native print function - prints values with their types
QValue func_print(QContext *ctx, const std::vector<QValue> &args) {
  std::cout << "[PRINT] ";
  for (size_t i = 0; i < args.size(); i++) {
    const auto &arg = args[i];
    std::string typeStr = GetValueTypeName(arg);
    std::string valStr;

    // Get raw value without quotes for strings
    if (std::holds_alternative<std::string>(arg)) {
      valStr = std::get<std::string>(arg);
    } else {
      valStr = ValueToString(arg);
    }

    std::cout << valStr << " (" << typeStr << ")";
    if (i < args.size() - 1)
      std::cout << ", ";
  }
  std::cout << std::endl;
  return std::monostate{};
}

//

QLangDomain *QLangDomain::m_QLang = nullptr;

QLangDomain::QLangDomain() {

  m_QLang = this;
  std::cout << "Creating QLang domain" << std::endl;

  auto errorCollector = std::make_shared<QErrorCollector>();

  m_Context = std::make_shared<QContext>("engine");
  m_Context->AddFunc("printf", func_printf);
  m_Context->AddFunc("print", func_print);
  m_Context->AddFunc("NodeTurn", node_turn);
  m_Context->AddFunc("NodeSetPosition", node_setPosition);
  m_Runner = make_shared<QRunner>(m_Context, errorCollector);

  LoadAndRegisterFolder("engine/qlang/classes");

  // exit(1);
}

bool QLangDomain::LoadAndRegister(std::string path) {
  auto errorCollector = std::make_shared<QErrorCollector>();

  Tokenizer tokenizer(path, errorCollector);
  tokenizer.Tokenize();
  if (errorCollector->HasErrors()) {
    std::cout << "Tokenization failed:" << std::endl;
    errorCollector->ListErrors();
    return false;
  }

  Parser parser(tokenizer.GetTokens(), errorCollector);
  auto program = parser.Parse();

  if (errorCollector->HasErrors()) {
    std::cout << "Parsing failed:" << std::endl;
    errorCollector->ListErrors(true);
    return false;
  }

  m_Runner->Run(program); // This registers all classes

  return true;
}

int QLangDomain::LoadAndRegisterFolder(const std::string &folderPath) {
  namespace fs = std::filesystem;

  int loadedCount = 0;

  if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
#if QLANG_DEBUG
    std::cout
        << "[QLangDomain] Error: Path does not exist or is not a directory: "
        << folderPath << std::endl;
#endif
    return 0;
  }

#if QLANG_DEBUG
  std::cout << "[QLangDomain] Scanning folder: " << folderPath << std::endl;
#endif

  for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
    if (entry.is_regular_file() && entry.path().extension() == ".q") {
      std::string filePath = entry.path().string();
#if QLANG_DEBUG
      std::cout << "[QLangDomain] Loading: " << filePath << std::endl;
#endif

      if (LoadAndRegister(filePath)) {
        loadedCount++;
      } else {
#if QLANG_DEBUG
        std::cout << "[QLangDomain] Failed to load: " << filePath << std::endl;
#endif
      }
    }
  }

#if QLANG_DEBUG
  std::cout << "[QLangDomain] Loaded " << loadedCount << " QLang file(s) from "
            << folderPath << std::endl;
#endif

  return loadedCount;
}

std::shared_ptr<QClassInstance>
QLangDomain::LoadClass(std::string path, Quantum::GraphNode *node) {
  namespace fs = std::filesystem;

  // Extract just the filename without folders or extension
  std::string className = fs::path(path).stem().string();

#if QLANG_DEBUG
  std::cout << "[QLangDomain] LoadClass: path=" << path
            << " className=" << className << std::endl;
#endif
  LoadAndRegister(path);

  auto cls = m_Runner->CreateInstance(className);

  cls->SetMember("NodePtr", static_cast<void *>(node));

  return cls;
}

void QLangDomain::RunMethod(std::shared_ptr<QClassInstance> inst,
                            const std::string &meth,
                            const std::vector<QValue> &args) {

  m_Runner->CallMethod(inst, meth, args);
}