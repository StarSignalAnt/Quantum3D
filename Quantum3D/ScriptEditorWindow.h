#pragma once

#include "CodeEditor.h"
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTabWidget>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class QTimer;

namespace Quantum {

class ScriptEditorWindow : public QMainWindow {
  Q_OBJECT

public:
  ScriptEditorWindow(QWidget *parent = nullptr);
  ~ScriptEditorWindow();

  // Opens a script file in a new or existing tab
  void OpenFile(const std::string &path);

  // Logs text to the script editor's console
  void LogConsole(const std::string &text);

private slots:
  void OnTabCloseRequested(int index);
  void OnCurrentTabChanged(int index);
  void OnTextChanged();
  void OnCompileTimerTimeout();

private:
  struct TabData {
    std::string path;
    class QTimer *compileTimer;
  };

  void setupUI();
  void createNewTab(const std::string &path, const std::string &content);
  void compileScript(CodeEditor *editor);
  void loadEngineClasses(); // Pre-load class names from engine/qlang/classes

  QTabWidget *m_tabWidget;
  QPlainTextEdit *m_consoleOutput;
  QDockWidget *m_consoleDock;

  // Map of path to tab index
  std::unordered_map<std::string, int> m_openFiles;
  // Map of editor to tab data
  std::unordered_map<CodeEditor *, TabData> m_tabData;
  // Known engine class names (Vec3, Mat4, etc.)
  std::set<std::string> m_engineClassNames;
  // Full engine class definitions for IntelliSense
  std::vector<QLangClassDef> m_engineClassDefs;

  // Register engine classes with an editor's symbol collector
  void registerEngineClassesWithEditor(CodeEditor *editor);
};

} // namespace Quantum
