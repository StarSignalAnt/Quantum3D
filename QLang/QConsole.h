#pragma once

#include <functional>
#include <iostream>
#include <string>

// Console output levels
enum class QConsoleLevel { Info = 0, Warning = 1, Error = 2, Debug = 3 };

// Delegate type for console output
// Parameters: message, level
using QConsolePrintDelegate =
    std::function<void(const std::string &, QConsoleLevel)>;

// QConsole - Global console output system with delegate support
// Allows external systems (like Qt) to subscribe to console output
class QConsole {
public:
  // Set the delegate that will receive all console output
  // The delegate is called in addition to stdout/stderr output
  static void SetPrintDelegate(QConsolePrintDelegate delegate) {
    s_PrintDelegate = delegate;
  }

  // Clear the print delegate
  static void ClearPrintDelegate() { s_PrintDelegate = nullptr; }

  // Print info message (white/normal)
  static void Print(const std::string &message) {
    std::cout << message << std::endl;
    if (s_PrintDelegate) {
      s_PrintDelegate(message, QConsoleLevel::Info);
    }
  }

  // Print warning message (yellow)
  static void PrintWarning(const std::string &message) {
    std::cout << "[WARNING] " << message << std::endl;
    if (s_PrintDelegate) {
      s_PrintDelegate(message, QConsoleLevel::Warning);
    }
  }

  // Print error message (red)
  static void PrintError(const std::string &message) {
    std::cerr << "[ERROR] " << message << std::endl;
    if (s_PrintDelegate) {
      s_PrintDelegate(message, QConsoleLevel::Error);
    }
  }

  // Print debug message (gray/dim) - only in debug builds
  static void PrintDebug(const std::string &message) {
#if QLANG_DEBUG || _DEBUG
    std::cout << "[DEBUG] " << message << std::endl;
    if (s_PrintDelegate) {
      s_PrintDelegate(message, QConsoleLevel::Debug);
    }
#else
    (void)message; // Suppress unused warning in release
#endif
  }

private:
  static inline QConsolePrintDelegate s_PrintDelegate = nullptr;
};
