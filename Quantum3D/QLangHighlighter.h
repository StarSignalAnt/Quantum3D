#pragma once

#include "../QLang/QError.h"
#include "../QLang/Tokenizer.h"
#include "CodeEditorTheme.h"
#include <QtGui/QSyntaxHighlighter>
#include <QtGui/QTextDocument>
#include <memory>


namespace Quantum {

class QLangHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit QLangHighlighter(QTextDocument *parent,
                            std::shared_ptr<CodeEditorTheme> theme);

  void setTheme(std::shared_ptr<CodeEditorTheme> theme);

protected:
  void highlightBlock(const QString &text) override;

private:
  std::shared_ptr<CodeEditorTheme> m_theme;

  // Map TokenType to our color type
  TokenColorType getColorType(TokenType tokenType) const;
};

} // namespace Quantum
