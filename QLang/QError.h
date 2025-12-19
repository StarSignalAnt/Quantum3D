#pragma once

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Error severity levels
enum class QErrorSeverity {
  Warning, // Non-fatal issue, execution continues
  Error,   // Problem that may affect execution
  Fatal    // Stops execution
};

// Structured error for IDE integration
struct QError {
  QErrorSeverity severity;
  std::string message;
  int line;
  int column;
  int length;          // Length of the token/segment to highlight
  std::string source;  // "tokenizer", "parser", "runtime"
  std::string context; // Function/method name for stack context

  // Helper to get severity as string
  std::string GetSeverityString() const {
    switch (severity) {
    case QErrorSeverity::Warning:
      return "Warning";
    case QErrorSeverity::Error:
      return "Error";
    case QErrorSeverity::Fatal:
      return "Fatal";
    default:
      return "Unknown";
    }
  }

  // Format error for display
  std::string ToString() const {
    std::ostringstream ss;
    ss << "[" << GetSeverityString() << "] ";
    if (line > 0) {
      ss << "Line " << line;
      if (column > 0) {
        ss << ":" << column;
      }
      ss << " - ";
    }
    ss << message;
    if (!context.empty()) {
      ss << " (in " << context << ")";
    }
    return ss.str();
  }
};

// Call stack entry for runtime stack traces
struct QStackFrame {
  std::string functionName;
  std::string className;
  int line;

  std::string ToString() const {
    std::ostringstream ss;
    if (!className.empty()) {
      ss << className << ".";
    }
    ss << functionName << "()";
    if (line > 0) {
      ss << " at line " << line;
    }
    return ss.str();
  }
};

// Call stack for runtime error context
class QCallStack {
public:
  void Push(const std::string &functionName, const std::string &className = "",
            int line = 0) {
    m_Frames.push_back({functionName, className, line});
  }

  void Pop() {
    if (!m_Frames.empty()) {
      m_Frames.pop_back();
    }
  }

  void Clear() { m_Frames.clear(); }

  bool IsEmpty() const { return m_Frames.empty(); }

  // Get formatted stack trace
  std::string GetStackTrace() const {
    if (m_Frames.empty()) {
      return "";
    }
    std::ostringstream ss;
    ss << "Stack trace:" << std::endl;
    for (int i = static_cast<int>(m_Frames.size()) - 1; i >= 0; i--) {
      ss << "  " << (m_Frames.size() - i) << ". " << m_Frames[i].ToString()
         << std::endl;
    }
    return ss.str();
  }

  // Get current context (innermost frame)
  std::string GetCurrentContext() const {
    if (m_Frames.empty()) {
      return "";
    }
    return m_Frames.back().ToString();
  }

private:
  std::vector<QStackFrame> m_Frames;
};

// Error collector - central place to collect and report all errors
class QErrorCollector {
public:
  QErrorCollector() = default;

  // Set source code for context printing
  void SetSource(const std::string &source) {
    m_SourceLines.clear();
    std::stringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) {
      m_SourceLines.push_back(line);
    }
  }

  // Register a context range (e.g. function body)
  void RegisterContext(const std::string &name, int startLine, int endLine) {
    m_ContextRanges[name] = {startLine, endLine};
  }

  // Report an error
  void ReportError(QErrorSeverity severity, const std::string &message,
                   int line = 0, int column = 0, int length = 0,
                   const std::string &source = "",
                   const std::string &context = "") {
    QError error{severity, message, line, column, length, source, context};
    m_Errors.push_back(error);

    if (severity == QErrorSeverity::Warning)
      m_WarningCount++;
    else if (severity == QErrorSeverity::Error)
      m_ErrorCount++;
    else if (severity == QErrorSeverity::Fatal)
      m_FatalCount++;
  }

  // Report with stack trace
  void ReportRuntimeError(const std::string &message, const QCallStack &stack,
                          int line = 0, int column = 0, int length = 0) {
    std::string fullMessage = message;
    if (!stack.IsEmpty()) {
      fullMessage += "\n" + stack.GetStackTrace();
    }
    ReportError(QErrorSeverity::Error, fullMessage, line, column, length,
                "runtime", stack.GetCurrentContext());
  }

  // Print all errors to console
  void ListErrors(bool listErrorFunction = false) const {
    if (m_Errors.empty()) {
      std::cout << "No errors reported." << std::endl;
      return;
    }

    std::cout << "=== QLang Errors ===" << std::endl;
    std::cout << "Total: " << m_Errors.size() << " issue(s) - " << m_FatalCount
              << " fatal, " << m_ErrorCount << " error(s), " << m_WarningCount
              << " warning(s)" << std::endl;
    std::cout << std::endl;

    for (size_t i = 0; i < m_Errors.size(); i++) {
      const auto &error = m_Errors[i];
      std::cout << (i + 1) << ". " << error.ToString() << std::endl;

      // Note: We used to skip runtime errors, but user requested context for
      // them too. if (error.source == "runtime") continue;

      // Parse context for "Function: X of class type Y" header
      // Runtime context might have () e.g. "Test.Test()" -> strip parens for
      // lookup
      std::string ctxName = error.context;
      size_t parenPos = ctxName.find('(');
      if (parenPos != std::string::npos) {
        ctxName = ctxName.substr(0, parenPos);
      }

      std::string className, methodName;
      size_t dotPos = ctxName.find('.');
      if (dotPos != std::string::npos) {
        className = ctxName.substr(0, dotPos);
        methodName = ctxName.substr(dotPos + 1);
        std::cout << "   Function: " << methodName << " of class type "
                  << className << std::endl;
      } else if (!ctxName.empty()) {
        std::cout << "   Context: " << ctxName << std::endl;
      }

      // Print source context
      if (listErrorFunction && !ctxName.empty()) {
        if (m_ContextRanges.count(ctxName)) {
          // Found it
        } else {
          std::cout << "   [DEBUG] Context '" << ctxName
                    << "' not found in ranges." << std::endl;
          std::cout << "   [DEBUG] Available ranges: ";
          for (const auto &pair : m_ContextRanges)
            std::cout << "'" << pair.first << "', ";
          std::cout << std::endl;
        }
      }

      if (listErrorFunction && !ctxName.empty() &&
          m_ContextRanges.count(ctxName)) {
        auto range = m_ContextRanges.at(ctxName);
        int start = range.first;
        int end = range.second;

        // Print function body with line numbers
        std::cout << "   --------------------------------------------------"
                  << std::endl;
        for (int l = start; l <= end; l++) {
          if (l > 0 && l <= static_cast<int>(m_SourceLines.size())) {

            // Highlight logic: >> for line, [token] for token
            std::string lineStr = m_SourceLines[l - 1];

            if (l == error.line && error.column > 0 && error.length > 0) {
              // Insert brackets around the error part
              // Column is 1-based index
              int colIdx = error.column - 1;
              if (colIdx >= 0 && colIdx < static_cast<int>(lineStr.length())) {
                std::string part1 = lineStr.substr(0, colIdx);
                std::string tokenPart = lineStr.substr(colIdx, error.length);
                std::string part2 = lineStr.substr(colIdx + error.length);
                lineStr = part1 + "[" + tokenPart + "]" + part2;
              }
            }

            std::string prefix = (l == error.line) ? ">> " : "   ";
            std::cout << prefix << l << ": " << lineStr << std::endl;

            // Optional: still print the caret if length is small or
            // highlighting failed but user asked for brackets specifically.
            // I'll keep the caret for non-bracketed errors or as extra visual
            // aid? User said "highlight the exact token if and where possible
            // by enclosing it within brackets... in this case" I'll keep caret
            // if no brackets applied (length=0)
            if (l == error.line && error.column > 0 && error.length == 0) {
              std::cout << "      ";
              for (int c = 1; c < error.column + (l < 10 ? 3 : 4); c++)
                std::cout << " "; // Rough alignment
              std::cout << "^" << std::endl;
            }
          }
        }
        std::cout << "   --------------------------------------------------"
                  << std::endl;
      } else if (error.line > 0 &&
                 error.line <= static_cast<int>(m_SourceLines.size())) {
        // Fallback: Print single line
        std::string lineStr = m_SourceLines[error.line - 1];
        if (error.column > 0 && error.length > 0) {
          int colIdx = error.column - 1;
          if (colIdx >= 0 && colIdx < static_cast<int>(lineStr.length())) {
            std::string part1 = lineStr.substr(0, colIdx);
            std::string tokenPart = lineStr.substr(colIdx, error.length);
            std::string part2 = lineStr.substr(colIdx + error.length);
            lineStr = part1 + "[" + tokenPart + "]" + part2;
          }
        }
        std::cout << "   " << error.line << ": " << lineStr << std::endl;
        if (error.column > 0 && error.length == 0) {
          std::cout << "   ";
          std::string formatting = std::to_string(error.line) + ": ";
          for (size_t c = 0; c < formatting.length() + error.column; c++)
            std::cout << " ";
          std::cout << "^" << std::endl;
        }
      }
      std::cout << std::endl;
    }
    std::cout << "====================" << std::endl;
  }

  // Get all errors for IDE integration
  const std::vector<QError> &GetErrors() const { return m_Errors; }

  // Check if there are any errors (not warnings)
  bool HasErrors() const { return m_ErrorCount > 0 || m_FatalCount > 0; }

  // Check if there are any issues at all
  bool HasAnyIssues() const { return !m_Errors.empty(); }

  // Get counts
  int GetErrorCount() const { return m_ErrorCount; }
  int GetWarningCount() const { return m_WarningCount; }
  int GetFatalCount() const { return m_FatalCount; }
  int GetTotalCount() const { return static_cast<int>(m_Errors.size()); }

  // Clear all errors
  void ClearErrors() {
    m_Errors.clear();
    m_ErrorCount = 0;
    m_WarningCount = 0;
    m_FatalCount = 0;
  }

private:
  std::vector<QError> m_Errors;
  std::vector<std::string> m_SourceLines;
  std::map<std::string, std::pair<int, int>>
      m_ContextRanges; // Name -> {start, end}
  int m_ErrorCount = 0;
  int m_WarningCount = 0;
  int m_FatalCount = 0;
};
