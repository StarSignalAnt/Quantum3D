#pragma once

#include "CodeEditorTheme.h"
#include "QLangSymbols.h"
#include <QtGui/QPainter>
#include <QtGui/QTextBlock>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QWidget>
#include <memory>

namespace Quantum {

class LineNumberArea;
class QLangHighlighter;

// Custom code editor with IDE-like features
class CodeEditor : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit CodeEditor(QWidget *parent = nullptr);

  void lineNumberAreaPaintEvent(QPaintEvent *event);
  int lineNumberAreaWidth();

  // Theme management
  void setTheme(std::shared_ptr<CodeEditorTheme> theme);
  std::shared_ptr<CodeEditorTheme> theme() const { return m_theme; }

  // IntelliSense
  void updateSymbols(); // Re-parse symbols from current text
  void setCompleter(QCompleter *completer);
  QCompleter *completer() const { return m_completer; }

  // Access symbol collector (for registering external classes)
  QLangSymbolCollector &symbolCollector() { return m_symbolCollector; }

signals:
  // Debug logging signal - connect to console output
  void debugLog(const QString &message);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;

private slots:
  void updateLineNumberAreaWidth(int newBlockCount);
  void highlightCurrentLine();
  void updateLineNumberArea(const QRect &rect, int dy);
  void insertCompletion(const QString &completion);

private:
  QWidget *m_lineNumberArea;
  std::shared_ptr<CodeEditorTheme> m_theme;
  QLangHighlighter *m_highlighter;

  // IntelliSense
  QCompleter *m_completer;
  QLangSymbolCollector m_symbolCollector;

  // Get the leading whitespace of a line
  QString getLineIndent(const QString &line);

  // Check if line ends with a character that should increase indent
  bool shouldIncreaseIndent(const QString &line);

  // Apply theme to editor
  void applyTheme();

  // IntelliSense helpers
  void setupCompleter();
  void updateCompletionModel();
  QString getWordUnderCursor() const;
  QString getCurrentClassName() const;
  QString getCurrentMethodName() const;
  void showCompletionPopup();

  // Dot-completion helpers
  QString getIdentifierBeforeDot() const;
  void showDotCompletion(const QString &variableName);

  // External class support
  bool m_dotCompletionMode = false; // True when showing dot-completion
};

// Line number area widget
class LineNumberArea : public QWidget {
public:
  explicit LineNumberArea(CodeEditor *editor)
      : QWidget(editor), m_codeEditor(editor) {}

  QSize sizeHint() const override {
    return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    m_codeEditor->lineNumberAreaPaintEvent(event);
  }

private:
  CodeEditor *m_codeEditor;
};

} // namespace Quantum
