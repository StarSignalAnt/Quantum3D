#include "ConsoleWidget.h"
#include <QtGui/QFont>
#include <QtWidgets/QScrollBar>

// Static instance
ConsoleWidget *ConsoleWidget::s_Instance = nullptr;

ConsoleWidget::ConsoleWidget(QWidget *parent) : QWidget(parent) {
  // Create layout
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Create toolbar with clear button
  QHBoxLayout *toolbarLayout = new QHBoxLayout();
  toolbarLayout->setContentsMargins(4, 2, 4, 2);

  m_clearButton = new QPushButton("Clear", this);
  m_clearButton->setFixedWidth(60);
  m_clearButton->setFixedHeight(22);
  connect(m_clearButton, &QPushButton::clicked, this, &ConsoleWidget::Clear);

  toolbarLayout->addWidget(m_clearButton);
  toolbarLayout->addStretch();
  mainLayout->addLayout(toolbarLayout);

  // Create text edit for console output
  m_textEdit = new QTextEdit(this);
  m_textEdit->setReadOnly(true);
  m_textEdit->setAcceptRichText(true);

  // Set monospace font
  QFont font("Consolas", 10);
  font.setStyleHint(QFont::Monospace);
  m_textEdit->setFont(font);

  // Dark background styling
  m_textEdit->setStyleSheet("QTextEdit {"
                            "  background-color: #1e1e1e;"
                            "  color: #d4d4d4;"
                            "  border: none;"
                            "  selection-background-color: #264f78;"
                            "}");

  mainLayout->addWidget(m_textEdit);
  setLayout(mainLayout);

  // Set as global instance if not already set
  if (!s_Instance) {
    s_Instance = this;
  }
}

ConsoleWidget::~ConsoleWidget() {
  if (s_Instance == this) {
    s_Instance = nullptr;
  }
}

ConsoleWidget *ConsoleWidget::Instance() { return s_Instance; }

void ConsoleWidget::SetInstance(ConsoleWidget *instance) {
  s_Instance = instance;
}

void ConsoleWidget::Print(const std::string &message) {
  QString html = QString("<span style='color:#d4d4d4;'>%1</span>")
                     .arg(QString::fromStdString(message).toHtmlEscaped());
  appendHtml(html);
}

void ConsoleWidget::PrintWarning(const std::string &message) {
  QString html = QString("<span style='color:#dcdcaa;'>[WARNING] %1</span>")
                     .arg(QString::fromStdString(message).toHtmlEscaped());
  appendHtml(html);
}

void ConsoleWidget::PrintError(const std::string &message) {
  QString html = QString("<span style='color:#f14c4c;'>[ERROR] %1</span>")
                     .arg(QString::fromStdString(message).toHtmlEscaped());
  appendHtml(html);
}

void ConsoleWidget::PrintDebug(const std::string &message) {
  QString html = QString("<span style='color:#808080;'>[DEBUG] %1</span>")
                     .arg(QString::fromStdString(message).toHtmlEscaped());
  appendHtml(html);
}

void ConsoleWidget::PrintWithLevel(const std::string &message, int level) {
  switch (level) {
  case 0: // Info
    Print(message);
    break;
  case 1: // Warning
    PrintWarning(message);
    break;
  case 2: // Error
    PrintError(message);
    break;
  case 3: // Debug
    PrintDebug(message);
    break;
  default:
    Print(message);
    break;
  }
}

void ConsoleWidget::Clear() { m_textEdit->clear(); }

void ConsoleWidget::appendHtml(const QString &html) {
  m_textEdit->append(html);

  // Auto-scroll to bottom
  QScrollBar *scrollBar = m_textEdit->verticalScrollBar();
  scrollBar->setValue(scrollBar->maximum());
}
