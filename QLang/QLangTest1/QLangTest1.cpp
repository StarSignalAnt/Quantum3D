#include <iostream>
#include <memory>
#include <variant>
#include <vector>

#include "Parser.h"
#include "QContext.h"
#include "QError.h"
#include "QRunner.h"
#include "Tokenizer.h"

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

class Test {
public:
  int x = 250;
};

int main() {
  std::cout << "=== QLang Engine Integration Demo ===" << std::endl;
  std::cout << std::endl;

  // Create shared error collector
  auto errorCollector = std::make_shared<QErrorCollector>();

  // ========== STEP 1: Parse the script ==========
  std::cout << "--- Step 1: Parsing Script ---" << std::endl;
  Tokenizer tokenizer("test/test.q", errorCollector);
  tokenizer.Tokenize();

  if (errorCollector->HasErrors()) {
    std::cout << "Tokenization failed:" << std::endl;
    errorCollector->ListErrors();
    return 1;
  }

  Parser parser(tokenizer.GetTokens(), errorCollector);
  auto program = parser.Parse();

  if (errorCollector->HasErrors()) {
    std::cout << "Parsing failed:" << std::endl;
    errorCollector->ListErrors(true);
    return 1;
  }

  // Print AST to show class structure
  std::cout << std::endl << "--- AST (Class Definitions) ---" << std::endl;
  program->Print();
  std::cout << std::endl;

  // ========== STEP 2: Create runner and register classes ==========
  std::cout << "--- Step 2: Setting up Runner ---" << std::endl;
  auto context = std::make_shared<QContext>("engine");
  context->AddFunc("printf", func_printf);
  context->AddFunc("print", func_print);

  QRunner runner(context, errorCollector);
  runner.Run(program); // This registers all classes

  std::cout << std::endl;

  // ========== STEP 3: Find classes (like the engine would) ==========
  std::cout << "--- Step 3: Finding Classes ---" << std::endl;

  auto gameNodeClass = runner.FindClass("GameNode");
  if (gameNodeClass) {
    std::cout << "Found class: GameNode" << std::endl;
  }

  auto playerClass = runner.FindClass("Player");
  if (playerClass) {
    std::cout << "Found class: Player (extends GameNode)" << std::endl;
  }

  std::cout << std::endl;

  // ========== STEP 4: Create instances ==========
  std::cout << "--- Step 4: Creating Instances ---" << std::endl;

  // Create a basic GameNode instance
  auto node1 = runner.CreateInstance("GameNode");

  std::cout << std::endl;

  // Create a Player instance (inherits from GameNode)
  auto player = runner.CreateInstance("Player");

  std::cout << std::endl;

  // ========== STEP 5: Simulate game loop - call methods ==========
  std::cout << "--- Step 5: Simulating Game Loop ---" << std::endl;

  // Simulate 3 frames of update/render with deltaTime
  float deltaTime = 0.016f; // ~60 FPS

  for (int frame = 0; frame < 3; frame++) {
    std::cout << std::endl << "=== Frame " << frame << " ===" << std::endl;

    // Create args vector with deltaTime for Update
    std::vector<QValue> updateArgs = {deltaTime};

    // Update GameNode with deltaTime and capture return value
    QValue result = runner.CallMethod(node1, "Update", updateArgs);
    if (std::holds_alternative<int32_t>(result)) {
      std::cout << "[C++] Update returned: " << std::get<int32_t>(result)
                << std::endl;
    }

    // Update Player (uses inherited Update from GameNode)
    runner.CallMethod(player, "Update", updateArgs);

    // Render GameNode
    //  runner.CallMethod(node1, "Render");

    // Render Player (uses overridden Render)
    // runner.CallMethod(player, "Render");

    // Simulate variable frame time
    deltaTime += 0.001f;
  }

  std::cout << std::endl;

  // ========== STEP 6: Call Player-specific method ==========
  std::cout << "--- Step 6: Player-specific Methods ---" << std::endl;

  // TakeDamage is defined only in Player
  std::vector<QValue> damageArgs = {static_cast<int32_t>(25)};
  runner.CallMethod(player, "TakeDamage", damageArgs);
  runner.CallMethod(player, "TakeDamage", damageArgs);
  runner.CallMethod(player, "Render");

  std::cout << std::endl;
  std::cout << "=== Demo Complete ===" << std::endl;

  return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started:
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add
//   Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project
//   and select the .sln file
