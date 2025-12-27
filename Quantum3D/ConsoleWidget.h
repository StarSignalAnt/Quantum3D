#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <string>

// Forward declaration
enum class QConsoleLevel;

// ConsoleWidget - Rich text console for QLang/Engine output
class ConsoleWidget : public QWidget {
public:
  explicit ConsoleWidget(QWidget *parent = nullptr);
  ~ConsoleWidget();

  // Singleton access for global console
  static ConsoleWidget *Instance();
  static void SetInstance(ConsoleWidget *instance);

  // Print methods with different severity levels
  void Print(const std::string &message);
  void PrintWarning(const std::string &message);
  void PrintError(const std::string &message);
  void PrintDebug(const std::string &message);

  // Print with explicit level
  void PrintWithLevel(const std::string &message, int level);

  // Clear console output
  void Clear();

  QSize sizeHint() const override { return QSize(800, 200); }

private:
  void appendHtml(const QString &html);

  QTextEdit *m_textEdit;
  QPushButton *m_clearButton;

  static ConsoleWidget *s_Instance;
};
