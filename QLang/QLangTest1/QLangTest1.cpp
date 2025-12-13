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
  std::cout << "=== QLang Test ===" << std::endl;
  std::cout << std::endl;

  // Create shared error collector
  auto errorCollector = std::make_shared<QErrorCollector>();

  // Tokenization
  std::cout << "--- Tokenization ---" << std::endl;
  Tokenizer tokenizer("test/test.q", errorCollector);
  tokenizer.Tokenize();

  if (errorCollector->HasErrors()) {
    std::cout << "Tokenization failed with errors:" << std::endl;
    errorCollector->ListErrors();
    return 1;
  }

  tokenizer.PrintTokens();
  std::cout << std::endl;

  // Parsing
  std::cout << "--- Parsing ---" << std::endl;
  Parser parser(tokenizer.GetTokens(), errorCollector);
  auto program = parser.Parse();

  if (errorCollector->HasErrors()) {
    std::cout << "Parsing failed with errors:" << std::endl;
    errorCollector->ListErrors(true); // Enable full function context
    return 1;
  }

  std::cout << std::endl;

  // Print AST
  std::cout << "--- AST ---" << std::endl;
  program->Print();

  std::cout << std::endl;

  // Create context and register native functions
  std::cout << "--- Setting up Context ---" << std::endl;
  auto context = std::make_shared<QContext>("program");

  // Add native functions
  context->AddFunc("printf", func_printf);
  context->AddFunc("print", func_print);

  std::cout << std::endl;

  // Run the program
  std::cout << "--- Running Program ---" << std::endl;
  QRunner runner(context, errorCollector);
  runner.Run(program);

  if (errorCollector->GetErrorCount() > 0) {
    std::cout << "Execution finished with errors:" << std::endl;
    errorCollector->ListErrors(true);
  } else if (errorCollector->GetWarningCount() > 0) {
    std::cout << "Execution finished with warnings:" << std::endl;
    errorCollector->ListErrors(true);
  }

  return 0;

  auto cls = runner.FindClassInstance("t1");

  Test *test = new Test;

  cls->SetMember("cls2", static_cast<void *>(test));

  if (cls) {
    auto member = cls->GetMember("cls2");

    // ALWAYS check before using std::get!
    if (std::holds_alternative<void *>(member)) {
      void *ptr = std::get<void *>(member);
      std::cout << "Got cptr: " << ptr << std::endl;
      Test *t = (Test *)ptr;
      std::cout << "TX:" << t->x << std::endl;
    } else if (std::holds_alternative<std::monostate>(member)) {
      std::cout << "Member is null/undefined" << std::endl;
    } else {
      std::cout << "Member is a different type" << std::endl;
    }
  } else {
    std::cout << "NO CLS" << std::endl;
  }

  std::cout << std::endl;

  // Show final context state
  std::cout << "--- Final Context State ---" << std::endl;
  context->PrintVariables();

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
