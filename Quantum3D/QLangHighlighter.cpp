#include "QLangHighlighter.h"

namespace Quantum {

QLangHighlighter::QLangHighlighter(QTextDocument *parent,
                                   std::shared_ptr<CodeEditorTheme> theme)
    : QSyntaxHighlighter(parent), m_theme(theme) {}

void QLangHighlighter::setTheme(std::shared_ptr<CodeEditorTheme> theme) {
  m_theme = theme;
  rehighlight();
}

TokenColorType QLangHighlighter::getColorType(TokenType tokenType) const {
  switch (tokenType) {
  // Keywords
  case TokenType::T_MODULE:
  case TokenType::T_END:
  case TokenType::T_CLASS:
  case TokenType::T_METHOD:
  case TokenType::T_NEW:
  case TokenType::T_RETURN:
  case TokenType::T_IF:
  case TokenType::T_ELSE:
  case TokenType::T_ELSEIF:
  case TokenType::T_FOR:
  case TokenType::T_TO:
  case TokenType::T_NEXT:
  case TokenType::T_WHILE:
  case TokenType::T_WEND:
  case TokenType::T_NULL:
    return TokenColorType::Keyword;

  // Types
  case TokenType::T_INT32:
  case TokenType::T_INT64:
  case TokenType::T_FLOAT32:
  case TokenType::T_FLOAT64:
  case TokenType::T_SHORT:
  case TokenType::T_STRING_TYPE:
  case TokenType::T_BOOL:
  case TokenType::T_VOID:
  case TokenType::T_CPTR:
    return TokenColorType::Type;

  // Booleans
  case TokenType::T_TRUE:
  case TokenType::T_FALSE:
    return TokenColorType::Boolean;

  // This keyword
  case TokenType::T_THIS:
    return TokenColorType::This;

  // Numbers
  case TokenType::T_INTEGER:
  case TokenType::T_FLOAT:
    return TokenColorType::Number;

  // Strings
  case TokenType::T_STRING:
    return TokenColorType::String;

  // Operators
  case TokenType::T_OPERATOR:
  case TokenType::T_LESS:
  case TokenType::T_GREATER:
    return TokenColorType::Operator;

  // Punctuation
  case TokenType::T_END_OF_LINE:
  case TokenType::T_COMMA:
  case TokenType::T_DOT:
  case TokenType::T_COLON:
  case TokenType::T_LPAREN:
  case TokenType::T_RPAREN:
  case TokenType::T_LBRACE:
  case TokenType::T_RBRACE:
  case TokenType::T_LBRACKET:
  case TokenType::T_RBRACKET:
    return TokenColorType::Punctuation;

  // Identifiers (variables, functions, etc.)
  case TokenType::T_IDENTIFIER:
    return TokenColorType::Identifier;

  // Default for unknown tokens
  case TokenType::T_EOF:
  case TokenType::T_UNKNOWN:
  default:
    return TokenColorType::Default;
  }
}

void QLangHighlighter::highlightBlock(const QString &text) {
  if (!m_theme || text.isEmpty())
    return;

  // Tokenize the line
  std::string lineText = text.toStdString();
  auto errorCollector = std::make_shared<QErrorCollector>();
  Tokenizer tokenizer(lineText, true, errorCollector);
  tokenizer.Tokenize();
  const auto &tokens = tokenizer.GetTokens();

  // Apply highlighting for each token
  for (const auto &token : tokens) {
    if (token.type == TokenType::T_EOF)
      continue;

    // Get color for this token type
    TokenColorType colorType = getColorType(token.type);
    QColor color = m_theme->getColor(colorType);

    QTextCharFormat format;
    format.setForeground(color);

    // Make keywords bold
    if (colorType == TokenColorType::Keyword ||
        colorType == TokenColorType::Type) {
      format.setFontWeight(QFont::Bold);
    }

    // Calculate position (column is 1-indexed)
    int start = token.column - 1;
    int length = static_cast<int>(token.value.length());

    // Ensure we don't go past the end of the line
    if (start >= 0 && start < text.length()) {
      length = qMin(length, static_cast<int>(text.length()) - start);
      setFormat(start, length, format);
    }
  }
}

} // namespace Quantum
