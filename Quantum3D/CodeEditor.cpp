#include "CodeEditor.h"
#include "QLangHighlighter.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QStringListModel>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QScrollBar>

namespace Quantum {

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent), m_completer(nullptr) {
  m_lineNumberArea = new LineNumberArea(this);

  // Connect signals for line number updates
  connect(this, &CodeEditor::blockCountChanged, this,
          &CodeEditor::updateLineNumberAreaWidth);
  connect(this, &CodeEditor::updateRequest, this,
          &CodeEditor::updateLineNumberArea);
  connect(this, &CodeEditor::cursorPositionChanged, this,
          &CodeEditor::highlightCurrentLine);

  updateLineNumberAreaWidth(0);
  highlightCurrentLine();

  // Set monospace font
  QFont font("Consolas", 11);
  font.setStyleHint(QFont::Monospace);
  font.setFixedPitch(true);
  setFont(font);

  // Set tab width (4 spaces)
  QFontMetrics metrics(font);
  setTabStopDistance(4 * metrics.horizontalAdvance(' '));

  // Create default theme (DarkUI)
  m_theme = std::make_shared<DarkUITheme>();

  // Create syntax highlighter
  m_highlighter = new QLangHighlighter(document(), m_theme);

  // Apply theme styling
  applyTheme();

  // Setup IntelliSense completer
  setupCompleter();
}

int CodeEditor::lineNumberAreaWidth() {
  int digits = 1;
  int max = qMax(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }
  // Minimum 3 digits, plus padding
  digits = qMax(digits, 3);
  int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
  return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
  if (dy)
    m_lineNumberArea->scroll(0, dy);
  else
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(),
                             rect.height());

  if (rect.contains(viewport()->rect()))
    updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);

  QRect cr = contentsRect();
  m_lineNumberArea->setGeometry(
      QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine() {
  QList<QTextEdit::ExtraSelection> extraSelections;

  if (!isReadOnly()) {
    QTextEdit::ExtraSelection selection;
    QColor lineColor = QColor("#2D2D2D");
    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);
  }

  setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
  QPainter painter(m_lineNumberArea);
  painter.fillRect(event->rect(), QColor("#252526"));

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top =
      qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      QString number = QString::number(blockNumber + 1);

      // Highlight current line number
      if (blockNumber == textCursor().blockNumber()) {
        painter.setPen(QColor("#C6C6C6"));
      } else {
        painter.setPen(QColor("#858585"));
      }

      painter.drawText(0, top, m_lineNumberArea->width() - 5,
                       fontMetrics().height(), Qt::AlignRight, number);
    }

    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

QString CodeEditor::getLineIndent(const QString &line) {
  QString indent;
  for (const QChar &ch : line) {
    if (ch == ' ' || ch == '\t') {
      indent += ch;
    } else {
      break;
    }
  }
  return indent;
}

bool CodeEditor::shouldIncreaseIndent(const QString &line) {
  QString trimmed = line.trimmed().toLower();
  // Increase indent after lines ending with these patterns (case-insensitive)
  return trimmed.endsWith("then") || trimmed.endsWith("do") ||
         trimmed.startsWith("method ") || trimmed.startsWith("class ") ||
         trimmed.startsWith("if ") || trimmed.startsWith("for ") ||
         trimmed.startsWith("while ") || trimmed.startsWith("else");
}

void CodeEditor::keyPressEvent(QKeyEvent *event) {
  // Handle completer keyboard events first
  if (m_completer && m_completer->popup()->isVisible()) {
    // Let completer handle these keys
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab:
      // Accept completion - get the selected item from popup
      if (m_completer->popup()->currentIndex().isValid()) {
        QString selectedText =
            m_completer->popup()->currentIndex().data().toString();
        insertCompletion(selectedText);
        m_completer->popup()->hide();
        return;
      }
      break;
    case Qt::Key_Escape:
      m_completer->popup()->hide();
      return;
    case Qt::Key_Up:
    case Qt::Key_Down:
      // Let completer handle navigation
      QCoreApplication::sendEvent(m_completer->popup(), event);
      return;
    default:
      break;
    }
  }

  // Handle Enter/Return - auto-indent
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    // Hide completer if visible
    if (m_completer && m_completer->popup()->isVisible()) {
      m_completer->popup()->hide();
    }

    QTextCursor cursor = textCursor();
    QString currentLine = cursor.block().text();
    QString indent = getLineIndent(currentLine);

    // Check if we should increase indent
    if (shouldIncreaseIndent(currentLine)) {
      indent += "\t";
    }

    // Insert newline and indent
    cursor.insertText("\n" + indent);
    setTextCursor(cursor);
    return;
  }

  // Handle Tab - insert tab or spaces
  if (event->key() == Qt::Key_Tab) {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
      // Indent selected lines
      int start = cursor.selectionStart();
      int end = cursor.selectionEnd();

      cursor.setPosition(start);
      cursor.movePosition(QTextCursor::StartOfBlock);
      cursor.setPosition(end, QTextCursor::KeepAnchor);
      cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

      QString selected = cursor.selectedText();
      // Replace paragraph separators with newlines for processing
      selected.replace(QChar::ParagraphSeparator, '\n');
      QStringList lines = selected.split('\n');

      for (int i = 0; i < lines.size(); ++i) {
        lines[i] = "\t" + lines[i];
      }

      cursor.insertText(lines.join('\n'));
    } else {
      cursor.insertText("\t");
    }
    return;
  }

  // Handle Shift+Tab - unindent
  if (event->key() == Qt::Key_Backtab) {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);

    QString firstChar = cursor.selectedText();
    if (firstChar == "\t" || firstChar == " ") {
      cursor.removeSelectedText();

      // If it was a space, try to remove up to 4 spaces (one tab worth)
      if (firstChar == " ") {
        for (int i = 0; i < 3; ++i) {
          QTextCursor check = textCursor();
          check.movePosition(QTextCursor::StartOfBlock);
          check.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
          if (check.selectedText() == " ") {
            check.removeSelectedText();
          } else {
            break;
          }
        }
      }
    }
    return;
  }

  // Get line content BEFORE the keypress for dedent detection
  QString lineBefore = textCursor().block().text();
  QString trimmedBefore = lineBefore.trimmed().toLower();

  // Default handling - this inserts the character
  QPlainTextEdit::keyPressEvent(event);

  // Auto-dedent: Check if we just completed a dedent keyword
  // Only process regular character keys (not modifiers, etc.)
  if (event->text().isEmpty())
    return;

  QTextCursor cursor = textCursor();
  QString lineAfter = cursor.block().text();
  QString trimmedAfter = lineAfter.trimmed().toLower();

  // List of dedent keywords
  static const QStringList dedentKeywords = {"end", "else", "elseif", "next",
                                             "wend"};

  // Check if:
  // 1. The line now matches a dedent keyword exactly
  // 2. The line BEFORE did NOT match (meaning we just completed it)
  bool nowMatchesKeyword = dedentKeywords.contains(trimmedAfter);
  bool beforeMatchedKeyword = dedentKeywords.contains(trimmedBefore);

  if (nowMatchesKeyword && !beforeMatchedKeyword) {
    // We just completed typing a dedent keyword - remove one indent level
    QString currentIndent = getLineIndent(lineAfter);
    QString indentBefore = getLineIndent(lineBefore);

    // Only dedent if:
    // 1. There's indentation to remove
    // 2. The indentation hasn't already been reduced (prevents double-dedent on
    // backspace+retype)
    if (!currentIndent.isEmpty() && currentIndent == indentBefore) {
      QString newIndent;
      if (currentIndent.endsWith('\t')) {
        newIndent = currentIndent.left(currentIndent.length() - 1);
      } else {
        // Remove up to 4 trailing spaces
        int spacesToRemove = qMin(4, currentIndent.length());
        newIndent = currentIndent.left(currentIndent.length() - spacesToRemove);
      }

      // Replace the line with reduced indentation
      cursor.movePosition(QTextCursor::StartOfBlock);
      cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      cursor.insertText(newIndent + lineAfter.trimmed());
      setTextCursor(cursor);
    }
  }

  // Trigger IntelliSense popup after typing
  if (m_completer && !event->text().isEmpty()) {
    // Check for dot-completion
    if (event->text() == ".") {
      QString varName = getIdentifierBeforeDot();
      if (!varName.isEmpty()) {
        showDotCompletion(varName);
        return;
      }
    }

    // Reset dot-completion mode for regular typing
    if (m_dotCompletionMode && event->text() != ".") {
      // Continue filtering in dot-completion mode
      QString word = getWordUnderCursor();
      m_completer->setCompletionPrefix(word);
      if (m_completer->completionCount() == 0) {
        m_completer->popup()->hide();
        m_dotCompletionMode = false;
      }
      return;
    }

    QString word = getWordUnderCursor();
    // Show popup after typing at least 2 characters
    if (word.length() >= 2) {
      m_dotCompletionMode = false;
      showCompletionPopup();
    } else {
      m_completer->popup()->hide();
    }
  }
}

void CodeEditor::setTheme(std::shared_ptr<CodeEditorTheme> theme) {
  m_theme = theme;
  m_highlighter->setTheme(theme);
  applyTheme();
}

void CodeEditor::applyTheme() {
  if (!m_theme)
    return;

  // Apply stylesheet
  QString style = QString("QPlainTextEdit {"
                          "  background-color: %1;"
                          "  color: %2;"
                          "  border: none;"
                          "  selection-background-color: %3;"
                          "  selection-color: %4;"
                          "}")
                      .arg(m_theme->backgroundColor().name())
                      .arg(m_theme->getColor(TokenColorType::Default).name())
                      .arg(m_theme->selectionBackground().name())
                      .arg(m_theme->selectionForeground().name());
  setStyleSheet(style);

  // Set palette
  QPalette p = palette();
  p.setColor(QPalette::Text, m_theme->getColor(TokenColorType::Default));
  p.setColor(QPalette::Base, m_theme->backgroundColor());
  setPalette(p);
}

// ========== IntelliSense Implementation ==========

void CodeEditor::setupCompleter() {
  m_completer = new QCompleter(this);
  m_completer->setWidget(this);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  m_completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_completer->setFilterMode(Qt::MatchContains);

  // Style the popup
  m_completer->popup()->setStyleSheet("QListView {"
                                      "  background-color: #252526;"
                                      "  color: #D4D4D4;"
                                      "  border: 1px solid #454545;"
                                      "  selection-background-color: #094771;"
                                      "  selection-color: #FFFFFF;"
                                      "  font-family: Consolas;"
                                      "  font-size: 11pt;"
                                      "}");

  connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
          this, &CodeEditor::insertCompletion);
}

void CodeEditor::setCompleter(QCompleter *completer) {
  if (m_completer)
    m_completer->disconnect(this);

  m_completer = completer;

  if (!m_completer)
    return;

  m_completer->setWidget(this);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  m_completer->setCaseSensitivity(Qt::CaseInsensitive);

  connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
          this, &CodeEditor::insertCompletion);
}

void CodeEditor::updateSymbols() {
  m_symbolCollector.parse(toPlainText());
  updateCompletionModel();
}

void CodeEditor::updateCompletionModel() {
  QStringList completions;

  // Add QLang keywords
  completions << QLangSymbolCollector::getKeywords();

  // Add type keywords
  completions << QLangSymbolCollector::getTypeKeywords();

  // Get current context
  QString className = getCurrentClassName();
  QString methodName = getCurrentMethodName();

  if (!className.isEmpty()) {
    // Add class methods
    for (const auto &sym : m_symbolCollector.getClassMethods(className)) {
      completions << sym.name;
    }

    // Add class members
    for (const auto &sym : m_symbolCollector.getClassMembers(className)) {
      completions << sym.name;
    }

    if (!methodName.isEmpty()) {
      // Add local variables
      for (const auto &sym :
           m_symbolCollector.getLocalVariables(className, methodName)) {
        completions << sym.name;
      }

      // Add method parameters
      for (const auto &sym :
           m_symbolCollector.getMethodParameters(className, methodName)) {
        completions << sym.name;
      }
    }
  }

  // Add all class names
  completions << m_symbolCollector.getClassNames();

  // Remove duplicates and sort
  completions.removeDuplicates();
  completions.sort(Qt::CaseInsensitive);

  m_completer->setModel(new QStringListModel(completions, m_completer));
}

QString CodeEditor::getWordUnderCursor() const {
  QTextCursor cursor = textCursor();
  cursor.select(QTextCursor::WordUnderCursor);
  return cursor.selectedText();
}

QString CodeEditor::getCurrentClassName() const {
  // Find the most recent "class ClassName" before cursor that hasn't been
  // closed
  QString text = toPlainText().left(textCursor().position());

  QString currentClass;
  int classDepth = 0; // Track class depth separately from other blocks

  // Match class, method, if, for, while, and end keywords
  QRegularExpression blockRe(R"(\b(class|method|if|for|while|end)\b)");
  QRegularExpressionMatchIterator it = blockRe.globalMatch(text);

  int totalDepth = 0; // Total block depth (class + method + if + for + while)
  int classBlockDepth = 0; // Depth when current class was entered

  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    QString keyword = match.captured(1);

    if (keyword == "class") {
      // Extract class name
      QRegularExpression classNameRe(R"(\bclass\s+(\w+))");
      QRegularExpressionMatch nameMatch =
          classNameRe.match(text, match.capturedStart());
      if (nameMatch.hasMatch()) {
        currentClass = nameMatch.captured(1);
        classBlockDepth = totalDepth;
      }
      totalDepth++;
    } else if (keyword == "method" || keyword == "if" || keyword == "for" ||
               keyword == "while") {
      totalDepth++;
    } else if (keyword == "end") {
      totalDepth--;
      // If we've closed back to before where the class started, we're outside
      // the class
      if (totalDepth <= classBlockDepth && !currentClass.isEmpty()) {
        // This end closes the class
        currentClass.clear();
        classBlockDepth = 0;
      }
    }
  }

  return currentClass;
}

QString CodeEditor::getCurrentMethodName() const {
  // Find the most recent "method Type Name" before cursor within current class
  QString text = toPlainText().left(textCursor().position());

  // Simple approach: find last "method" keyword and extract name
  QRegularExpression methodRe(R"(\bmethod\s+\w+\s+(\w+)\s*\()");
  QRegularExpressionMatchIterator it = methodRe.globalMatch(text);

  QString lastMethod;
  int lastMethodEnd = -1;

  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    lastMethod = match.captured(1);
    lastMethodEnd = match.capturedEnd();
  }

  // Check if we've passed an 'end' that closes this method
  if (lastMethodEnd > 0) {
    QString afterMethod = text.mid(lastMethodEnd);
    int methodDepth = 1;

    QRegularExpression blockRe(R"(\b(method|if|for|while|end)\b)");
    QRegularExpressionMatchIterator blockIt = blockRe.globalMatch(afterMethod);

    while (blockIt.hasNext() && methodDepth > 0) {
      QRegularExpressionMatch match = blockIt.next();
      QString keyword = match.captured(1).toLower();

      if (keyword == "method" || keyword == "if" || keyword == "for" ||
          keyword == "while") {
        methodDepth++;
      } else if (keyword == "end") {
        methodDepth--;
      }
    }

    if (methodDepth <= 0) {
      return QString(); // Method has been closed
    }
  }

  return lastMethod;
}

void CodeEditor::showCompletionPopup() {
  updateSymbols();

  QString prefix = getWordUnderCursor();
  m_completer->setCompletionPrefix(prefix);

  if (m_completer->completionCount() == 0) {
    m_completer->popup()->hide();
    return;
  }

  QRect rect = cursorRect();
  rect.setWidth(m_completer->popup()->sizeHintForColumn(0) +
                m_completer->popup()->verticalScrollBar()->sizeHint().width());
  m_completer->complete(rect);
}

void CodeEditor::insertCompletion(const QString &completion) {
  if (m_completer->widget() != this)
    return;

  QTextCursor cursor = textCursor();
  int extra = completion.length() - m_completer->completionPrefix().length();

  cursor.movePosition(QTextCursor::Left);
  cursor.movePosition(QTextCursor::EndOfWord);
  cursor.insertText(completion.right(extra));
  setTextCursor(cursor);
}

void CodeEditor::focusInEvent(QFocusEvent *event) {
  if (m_completer)
    m_completer->setWidget(this);
  QPlainTextEdit::focusInEvent(event);
}

QString CodeEditor::getIdentifierBeforeDot() const {
  QTextCursor cursor = textCursor();
  int pos = cursor.position() - 1; // Position before the dot we just typed

  if (pos <= 0)
    return QString();

  QString text = toPlainText();

  // Move back past the dot
  pos--;

  // Skip any whitespace
  while (pos >= 0 && text[pos].isSpace()) {
    pos--;
  }

  if (pos < 0)
    return QString();

  // Extract the FULL member access chain (e.g., "light1.Cam" not just "Cam")
  // Work backwards collecting identifiers and dots
  int endPos = pos + 1;

  while (pos >= 0) {
    // Collect identifier
    while (pos >= 0 && (text[pos].isLetterOrNumber() || text[pos] == '_')) {
      pos--;
    }

    // Check if there's a dot before this identifier (part of a chain)
    int checkPos = pos;
    while (checkPos >= 0 && text[checkPos].isSpace()) {
      checkPos--;
    }

    if (checkPos >= 0 && text[checkPos] == '.') {
      // There's a dot, continue extracting the chain
      pos = checkPos - 1;
      // Skip whitespace before the dot
      while (pos >= 0 && text[pos].isSpace()) {
        pos--;
      }
    } else {
      // No more dots in the chain, we're done
      break;
    }
  }

  return text.mid(pos + 1, endPos - pos - 1).trimmed();
}

void CodeEditor::showDotCompletion(const QString &memberAccessChain) {
  emit debugLog("=== showDotCompletion called ===");
  emit debugLog("Member access chain: " + memberAccessChain);

  // First, update symbols to have the latest state
  m_symbolCollector.parse(toPlainText());

  // Get current context
  QString className = getCurrentClassName();
  QString methodName = getCurrentMethodName();
  emit debugLog("Context - class: " + className + ", method: " + methodName);

  // Split the chain by dots (e.g., "light1.Cam" -> ["light1", "Cam"])
  QStringList parts = memberAccessChain.split('.', Qt::SkipEmptyParts);
  if (parts.isEmpty())
    return;

  emit debugLog("Chain parts: " + parts.join(" -> "));

  // Resolve the type step by step
  QString currentType;

  for (int i = 0; i < parts.size(); ++i) {
    const QString &part = parts[i];

    if (i == 0) {
      // First part is a variable name - look it up
      currentType =
          m_symbolCollector.getVariableType(part, className, methodName);
      emit debugLog(
          "Step " + QString::number(i) + ": Variable '" + part +
          "' -> type: " + (currentType.isEmpty() ? "(empty)" : currentType));

      if (currentType.isEmpty()) {
        // Maybe it's 'this'
        if (part == "this") {
          currentType = className;
          emit debugLog("  Recognized 'this', using class: " + currentType);
        }
      }
    } else {
      // Subsequent parts are member accesses - look up member type in current
      // type
      if (currentType.isEmpty()) {
        emit debugLog("Step " + QString::number(i) +
                      ": Cannot resolve member '" + part +
                      "' - previous type was empty");
        break;
      }

      // Get the type of this member from the current type
      QString memberType = m_symbolCollector.getMemberType(currentType, part);
      emit debugLog("Step " + QString::number(i) + ": Member '" + part +
                    "' in class '" + currentType + "' -> type: " +
                    (memberType.isEmpty() ? "(empty)" : memberType));

      currentType = memberType;
    }

    if (currentType.isEmpty()) {
      emit debugLog("Chain resolution failed at step " + QString::number(i));
      break;
    }
  }

  emit debugLog("Final resolved type: " +
                (currentType.isEmpty() ? "(empty)" : currentType));

  // Get typed members for this type (members first, then methods)
  QList<QLangSymbolCollector::CompletionItem> items;
  if (!currentType.isEmpty()) {
    items = m_symbolCollector.getTypedMembersForType(currentType);
    emit debugLog("Found " + QString::number(items.size()) +
                  " completions for " + currentType);
  }

  if (items.isEmpty()) {
    // No members found, hide popup
    emit debugLog("No members found, hiding popup");
    m_completer->popup()->hide();
    return;
  }

  // Create model with icons
  QStandardItemModel *model = new QStandardItemModel(m_completer);

  // Create icons programmatically - colored circles
  auto createIcon = [](const QColor &color) -> QIcon {
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(color.darker(120));
    painter.drawEllipse(2, 2, 12, 12);
    return QIcon(pixmap);
  };

  QIcon memberIcon = createIcon(QColor(70, 130, 180)); // Steel blue for members
  QIcon methodIcon =
      createIcon(QColor(138, 43, 226)); // Blue violet for methods

  for (const auto &item : items) {
    QStandardItem *stdItem = new QStandardItem(item.name);
    stdItem->setIcon(item.isMethod ? methodIcon : memberIcon);
    model->appendRow(stdItem);
  }

  emit debugLog("Showing " + QString::number(items.size()) + " completions");
  m_completer->setModel(model);
  m_completer->setCompletionPrefix("");

  // Show popup
  m_dotCompletionMode = true;

  QRect rect = cursorRect();
  rect.setWidth(m_completer->popup()->sizeHintForColumn(0) +
                m_completer->popup()->verticalScrollBar()->sizeHint().width() +
                30);
  m_completer->complete(rect);
}

} // namespace Quantum
