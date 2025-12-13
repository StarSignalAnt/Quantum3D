// QLangTest1.cpp : This file contains the 'main' function. Program execution
// begins and ends there.
//

#include "Parser.h"
#include "QContext.h"
#include "QRunner.h"
#include "Tokenizer.h"
#include <iostream>

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

int main() {
  std::cout << "=== QLang Test ===" << std::endl;
  std::cout << std::endl;

  // Tokenization
  std::cout << "--- Tokenization ---" << std::endl;
  Tokenizer tokenizer("test/test.q");
  tokenizer.Tokenize();
  tokenizer.PrintTokens();

  std::cout << std::endl;

  // Parsing
  std::cout << "--- Parsing ---" << std::endl;
  Parser parser(tokenizer.GetTokens());
  auto program = parser.Parse();

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
  QRunner runner(context);
  runner.Run(program);

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
