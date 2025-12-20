#pragma once

#include <QString>
#include <QtGui/QColor>
#include <unordered_map>


namespace Quantum {

// Color categories for code tokens
enum class TokenColorType {
  Default,     // Unknown/unrecognized tokens
  Keyword,     // class, method, if, else, for, while, return, new, etc.
  Type,        // int32, float32, string, bool, etc.
  Identifier,  // Variable/function names
  Number,      // Numeric literals
  String,      // String literals
  Operator,    // +, -, *, /, =, ==, etc.
  Comment,     // Comments (future)
  ClassName,   // Class names
  MethodName,  // Method names
  Punctuation, // Parentheses, braces, commas
  Boolean,     // true, false
  This         // 'this' keyword
};

// Theme class for code editor colors
class CodeEditorTheme {
public:
  CodeEditorTheme() = default;
  virtual ~CodeEditorTheme() = default;

  // Get color for a token type
  QColor getColor(TokenColorType type) const {
    auto it = m_colors.find(type);
    if (it != m_colors.end()) {
      return it->second;
    }
    return m_colors.at(TokenColorType::Default);
  }

  // Background color
  QColor backgroundColor() const { return m_backgroundColor; }

  // Current line highlight color
  QColor lineHighlightColor() const { return m_lineHighlightColor; }

  // Line number colors
  QColor lineNumberColor() const { return m_lineNumberColor; }
  QColor lineNumberActiveColor() const { return m_lineNumberActiveColor; }
  QColor lineNumberBackground() const { return m_lineNumberBackground; }

  // Selection colors
  QColor selectionBackground() const { return m_selectionBackground; }
  QColor selectionForeground() const { return m_selectionForeground; }

protected:
  std::unordered_map<TokenColorType, QColor> m_colors;
  QColor m_backgroundColor;
  QColor m_lineHighlightColor;
  QColor m_lineNumberColor;
  QColor m_lineNumberActiveColor;
  QColor m_lineNumberBackground;
  QColor m_selectionBackground;
  QColor m_selectionForeground;
};

// Dark UI Theme - VS Code inspired
class DarkUITheme : public CodeEditorTheme {
public:
  DarkUITheme() {
    // Token colors
    m_colors[TokenColorType::Default] = QColor("#D4D4D4"); // Light gray
    m_colors[TokenColorType::Keyword] =
        QColor("#C586C0"); // Purple/pink (if, else, for, etc.)
    m_colors[TokenColorType::Type] =
        QColor("#4EC9B0"); // Teal (int32, float32, etc.)
    m_colors[TokenColorType::Identifier] = QColor("#9CDCFE");  // Light blue
    m_colors[TokenColorType::Number] = QColor("#B5CEA8");      // Light green
    m_colors[TokenColorType::String] = QColor("#CE9178");      // Orange/salmon
    m_colors[TokenColorType::Operator] = QColor("#D4D4D4");    // Light gray
    m_colors[TokenColorType::Comment] = QColor("#6A9955");     // Green
    m_colors[TokenColorType::ClassName] = QColor("#4EC9B0");   // Teal
    m_colors[TokenColorType::MethodName] = QColor("#DCDCAA");  // Yellow
    m_colors[TokenColorType::Punctuation] = QColor("#D4D4D4"); // Light gray
    m_colors[TokenColorType::Boolean] = QColor("#569CD6");     // Blue
    m_colors[TokenColorType::This] = QColor("#569CD6");        // Blue

    // Editor colors
    m_backgroundColor = QColor("#1E1E1E");
    m_lineHighlightColor = QColor("#2D2D2D");
    m_lineNumberColor = QColor("#858585");
    m_lineNumberActiveColor = QColor("#C6C6C6");
    m_lineNumberBackground = QColor("#252526");
    m_selectionBackground = QColor("#264F78");
    m_selectionForeground = QColor("#FFFFFF");
  }
};

} // namespace Quantum
