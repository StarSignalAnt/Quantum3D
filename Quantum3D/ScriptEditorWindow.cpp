#include "ScriptEditorWindow.h"
#include <QTimer>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QVBoxLayout>
#include <filesystem>
#include <fstream>
#include <sstream>

// QLang Includes (standalone parsing - no QLangDomain)
#include "../QLang/Parser.h"
#include "../QLang/QContext.h"
#include "../QLang/QError.h"
#include "../QLang/QRunner.h"
#include "../QLang/Tokenizer.h"

namespace Quantum {

ScriptEditorWindow::ScriptEditorWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("QLang Script Editor");
  resize(1000, 700);
  setupUI();
  loadEngineClasses(); // Pre-load known engine classes (Vec3, Mat4, etc.)
}

ScriptEditorWindow::~ScriptEditorWindow() {}

void ScriptEditorWindow::setupUI() {
  // Central Widget: Tab Widget
  m_tabWidget = new QTabWidget(this);
  m_tabWidget->setTabsClosable(true);
  m_tabWidget->setMovable(true);
  setCentralWidget(m_tabWidget);

  connect(m_tabWidget, &QTabWidget::tabCloseRequested, this,
          &ScriptEditorWindow::OnTabCloseRequested);

  // Console Dock
  m_consoleDock = new QDockWidget("Script Console", this);
  m_consoleOutput = new QPlainTextEdit(m_consoleDock);
  m_consoleOutput->setReadOnly(true);

  // Style the console
  m_consoleOutput->setStyleSheet(
      "QPlainTextEdit { background-color: #121212; color: #E0E0E0; "
      "font-family: 'Consolas', 'Monaco', monospace; font-size: 10pt; }");

  m_consoleDock->setWidget(m_consoleOutput);
  addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);

  // Initial styling for the main window
  setStyleSheet("QMainWindow { background-color: #1E1E1E; color: #D4D4D4; }"
                "QTabWidget::pane { border: 1px solid #333333; }"
                "QTabBar::tab { background: #2D2D2D; color: #AAAAAA; padding: "
                "8px 12px; border: 1px solid #333333; border-bottom: none; }"
                "QTabBar::tab:selected { background: #1E1E1E; color: #FFFFFF; "
                "border-bottom: 2px solid #0078D7; }"
                "QPlainTextEdit { background-color: #1E1E1E; color: #D4D4D4; "
                "font-family: 'Consolas', 'Monaco', monospace; font-size: "
                "11pt; border: none; }");
}

void ScriptEditorWindow::OpenFile(const std::string &path) {
  // If already open, switch to that tab
  auto it = m_openFiles.find(path);
  if (it != m_openFiles.end()) {
    m_tabWidget->setCurrentIndex(it->second);
    show();
    raise();
    activateWindow();
    return;
  }

  // Read file content
  std::ifstream file(path);
  if (!file.is_open()) {
    QMessageBox::warning(
        this, "Error", "Could not open file: " + QString::fromStdString(path));
    return;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  createNewTab(path, content);
  show();
  raise();
  activateWindow();
}

void ScriptEditorWindow::createNewTab(const std::string &path,
                                      const std::string &content) {
  CodeEditor *editor = new CodeEditor(m_tabWidget);
  editor->setPlainText(QString::fromStdString(content));

  // Get simple filename for tab title
  std::string filename = path;
  size_t lastSlash = path.find_last_of("\\/");
  if (lastSlash != std::string::npos) {
    filename = path.substr(lastSlash + 1);
  }

  int index = m_tabWidget->addTab(editor, QString::fromStdString(filename));
  m_tabWidget->setTabToolTip(index, QString::fromStdString(path));
  m_tabWidget->setCurrentIndex(index);

  m_openFiles[path] = index;

  // Initialize Tab Data (for compilation only - no runtime execution)
  TabData data;
  data.path = path;
  data.compileTimer = new QTimer(this);
  data.compileTimer->setSingleShot(true);
  data.compileTimer->setInterval(2000); // 2 seconds

  // Store editor pointer on timer to find it back
  data.compileTimer->setProperty("editor", QVariant::fromValue((void *)editor));

  m_tabData[editor] = data;

  // Connect signals
  connect(editor, &CodeEditor::textChanged, this,
          &ScriptEditorWindow::OnTextChanged);
  connect(data.compileTimer, &QTimer::timeout, this,
          &ScriptEditorWindow::OnCompileTimerTimeout);

  // Connect debug logging from editor to console
  connect(editor, &CodeEditor::debugLog, this, [this](const QString &msg) {
    LogConsole("[DEBUG] " + msg.toStdString());
  });

  // Register engine classes with this editor's symbol collector for
  // IntelliSense
  registerEngineClassesWithEditor(editor);

  // Initial compilation
  compileScript(editor);
}

void ScriptEditorWindow::LogConsole(const std::string &text) {
  m_consoleOutput->appendPlainText(QString::fromStdString(text));
  // Scroll to bottom
  m_consoleOutput->moveCursor(QTextCursor::End);
}

void ScriptEditorWindow::OnTabCloseRequested(int index) {
  QWidget *widget = m_tabWidget->widget(index);
  std::string pathToRemove;

  for (auto it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
    if (it->second == index) {
      pathToRemove = it->first;
      break;
    }
  }

  if (!pathToRemove.empty()) {
    m_openFiles.erase(pathToRemove);
  }

  // Cleanup Tab Data
  auto dataIt = m_tabData.find(static_cast<CodeEditor *>(widget));
  if (dataIt != m_tabData.end()) {
    dataIt->second.compileTimer->stop();
    delete dataIt->second.compileTimer;
    m_tabData.erase(dataIt);
  }

  m_tabWidget->removeTab(index);
  delete widget;

  // Update remaining tab indices in the map
  for (auto &pair : m_openFiles) {
    if (pair.second > index) {
      pair.second--;
    }
  }
}

void ScriptEditorWindow::OnCurrentTabChanged(int index) {
  Q_UNUSED(index);
  // Could update window title or save state here
}

void ScriptEditorWindow::OnTextChanged() {
  CodeEditor *editor = qobject_cast<CodeEditor *>(sender());
  if (editor && m_tabData.count(editor)) {
    m_tabData[editor].compileTimer->start();
  }
}

void ScriptEditorWindow::OnCompileTimerTimeout() {
  QTimer *timer = qobject_cast<QTimer *>(sender());
  if (timer) {
    CodeEditor *editor =
        (CodeEditor *)timer->property("editor").value<void *>();
    if (editor) {
      compileScript(editor);
    }
  }
}

void ScriptEditorWindow::compileScript(CodeEditor *editor) {
  namespace fs = std::filesystem;

  if (!editor || !m_tabData.count(editor))
    return;

  std::string path = m_tabData[editor].path;
  std::string code = editor->toPlainText().toStdString();

  LogConsole("--- Compiling: " + path + " ---");

  auto errorCollector = std::make_shared<QErrorCollector>();
  Tokenizer tokenizer(code, true, errorCollector);
  tokenizer.Tokenize();
  const auto &tokens = tokenizer.GetTokens();

  // Check for lexer errors first
  if (errorCollector->HasErrors()) {
    for (const auto &err : errorCollector->GetErrors()) {
      LogConsole("[" + err.GetSeverityString() + "] Line " +
                 std::to_string(err.line) + ": " + err.message);
    }
    return; // Don't continue to parsing if there are lexer errors
  }

  // Parse the user script code (name validation is deferred)
  Parser parser(tokens, errorCollector);
  parser.RegisterKnownClasses(m_engineClassNames); // For IntelliSense hints
  auto program = parser.Parse();

  // Check for parse errors
  if (errorCollector->HasErrors()) {
    for (const auto &err : errorCollector->GetErrors()) {
      LogConsole("[" + err.GetSeverityString() + "] Line " +
                 std::to_string(err.line) + ": " + err.message);
    }
    return;
  }

  // === Two-Phase Name Validation ===
  // Create QRunner with all engine classes registered first
  auto context = std::make_shared<QContext>();
  auto validationErrorCollector = std::make_shared<QErrorCollector>();
  QRunner runner(context, validationErrorCollector);

  // Phase 1: Register all engine classes
  runner.RegisterClasses(m_enginePrograms);

  // Phase 2: Parse and register ALL user scripts in the same folder
  // This enables cross-references between user-defined classes
  fs::path scriptPath(path);
  fs::path contentDir = scriptPath.parent_path();

  std::vector<std::shared_ptr<QProgram>> siblingPrograms;
  if (fs::exists(contentDir) && fs::is_directory(contentDir)) {
    for (const auto &entry : fs::directory_iterator(contentDir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".q") {
        std::string siblingPath = entry.path().string();

        // For the current file, use the already-parsed program
        if (siblingPath == path) {
          if (program) {
            siblingPrograms.push_back(program);
          }
          continue;
        }

        // Parse sibling script
        std::ifstream file(entry.path());
        if (!file.is_open())
          continue;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string siblingCode = buffer.str();

        auto siblingErrorCollector = std::make_shared<QErrorCollector>();
        Tokenizer siblingTokenizer(siblingCode, true, siblingErrorCollector);
        siblingTokenizer.Tokenize();

        if (!siblingErrorCollector->HasErrors()) {
          Parser siblingParser(siblingTokenizer.GetTokens(),
                               siblingErrorCollector);
          siblingParser.RegisterKnownClasses(m_engineClassNames);
          auto siblingProgram = siblingParser.Parse();

          if (siblingProgram && !siblingErrorCollector->HasErrors()) {
            siblingPrograms.push_back(siblingProgram);
          }
        }
      }
    }
  } else {
    // If we can't read the folder, just register the current program
    if (program) {
      siblingPrograms.push_back(program);
    }
  }

  // Register all user programs
  runner.RegisterClasses(siblingPrograms);

  LogConsole("Registered " + std::to_string(siblingPrograms.size()) +
             " user script(s) from content folder");

  // Phase 3: Validate name references in user script
  bool namesValid = true;
  if (program) {
    namesValid = runner.EnsureNames(program);
  }

  // Report validation errors
  if (!namesValid) {
    for (const auto &err : validationErrorCollector->GetErrors()) {
      LogConsole("[" + err.GetSeverityString() + "] " + err.message);
    }
  }

  // Report final status
  if (errorCollector->HasAnyIssues() || !namesValid) {
    int totalErrors = errorCollector->GetErrorCount() +
                      validationErrorCollector->GetErrorCount();
    int totalWarnings = errorCollector->GetWarningCount() +
                        validationErrorCollector->GetWarningCount();

    if (totalErrors > 0) {
      LogConsole("Compile failed with " + std::to_string(totalErrors) +
                 " error(s)");
    } else if (totalWarnings > 0) {
      LogConsole("Compiled with " + std::to_string(totalWarnings) +
                 " warning(s)");
    }
  } else {
    LogConsole("Compiled OK");
  }

  // On successful compile (no errors), extract class definitions for
  // IntelliSense from ALL registered programs (current + siblings)
  if (namesValid) {
    for (const auto &prog : siblingPrograms) {
      for (const auto &cls : prog->GetClasses()) {
        QLangClassDef classDef;
        classDef.name = QString::fromStdString(cls->GetName());
        classDef.parentClass =
            QString::fromStdString(cls->GetParentClassName());

        // Add members
        for (const auto &member : cls->GetMembers()) {
          QString memberName = QString::fromStdString(member->GetName());
          QString memberType = QString::fromStdString(member->GetTypeName());
          classDef.members << memberName;
          classDef.memberTypes[memberName] = memberType;
        }

        // Add methods
        for (const auto &method : cls->GetMethods()) {
          classDef.methods << QString::fromStdString(method->GetName());
        }

        // Register with this editor's symbol collector
        editor->symbolCollector().registerExternalClass(classDef);
      }
    }
  }
}

void ScriptEditorWindow::loadEngineClasses() {
  namespace fs = std::filesystem;

  // Get path relative to executable
  QString appDir = QCoreApplication::applicationDirPath();
  std::string engineClassPath =
      (appDir + "/engine/qlang/classes").toStdString();

  LogConsole("=== Loading Engine Classes ===");
  LogConsole("App dir: " + appDir.toStdString());
  LogConsole("Trying path: " + engineClassPath);

  if (!fs::exists(engineClassPath) || !fs::is_directory(engineClassPath)) {
    // Try alternate paths
    LogConsole("Primary path not found, trying: engine/qlang/classes");
    engineClassPath = "engine/qlang/classes";
    if (!fs::exists(engineClassPath) || !fs::is_directory(engineClassPath)) {
      // Engine classes folder not found - this is OK, just means no
      // pre-registered classes
      LogConsole("Engine classes folder not found!");
      return;
    }
  }

  LogConsole("Using path: " + engineClassPath);

  // Phase 1: Parse all engine .q files (no name validation yet)
  m_enginePrograms.clear();
  for (const auto &entry : fs::recursive_directory_iterator(engineClassPath)) {
    if (entry.is_regular_file() && entry.path().extension() == ".q") {
      std::ifstream file(entry.path());
      if (!file.is_open())
        continue;

      std::stringstream buffer;
      buffer << file.rdbuf();
      std::string content = buffer.str();

      // Tokenize and parse to extract class information
      auto errorCollector = std::make_shared<QErrorCollector>();
      Tokenizer tokenizer(content, true, errorCollector);
      tokenizer.Tokenize();
      const auto &tokens = tokenizer.GetTokens();

      if (!errorCollector->HasErrors()) {
        Parser parser(tokens, errorCollector);
        auto program = parser.Parse();

        if (program) {
          // Store the program for phase 2
          m_enginePrograms.push_back(program);

          // Extract class names for Parser preregistration
          for (const auto &cls : program->GetClasses()) {
            m_engineClassNames.insert(cls->GetName());
          }
        }
      }
    }
  }

  LogConsole("Phase 1: Parsed " + std::to_string(m_enginePrograms.size()) +
             " engine files");

  // Phase 2: Register all classes with QRunner
  auto context = std::make_shared<QContext>();
  auto errorCollector = std::make_shared<QErrorCollector>();
  QRunner runner(context, errorCollector);
  runner.RegisterClasses(m_enginePrograms);

  LogConsole("Phase 2: Registered " +
             std::to_string(m_engineClassNames.size()) + " engine classes");

  // Phase 3: Validate all name references
  bool allValid = true;
  for (const auto &program : m_enginePrograms) {
    if (!runner.EnsureNames(program)) {
      allValid = false;
    }
  }

  if (!allValid) {
    LogConsole("WARNING: Engine classes have validation errors:");
    for (const auto &err : errorCollector->GetErrors()) {
      LogConsole("  [" + err.GetSeverityString() + "] " + err.message);
    }
  } else {
    LogConsole("Phase 3: All engine class names validated OK");
  }

  // Build IntelliSense class definitions
  for (const auto &program : m_enginePrograms) {
    for (const auto &cls : program->GetClasses()) {
      QLangClassDef classDef;
      classDef.name = QString::fromStdString(cls->GetName());
      classDef.parentClass = QString::fromStdString(cls->GetParentClassName());

      // Add members
      for (const auto &member : cls->GetMembers()) {
        QString memberName = QString::fromStdString(member->GetName());
        QString memberType = QString::fromStdString(member->GetTypeName());
        classDef.members << memberName;
        classDef.memberTypes[memberName] = memberType;
      }

      // Add methods
      for (const auto &method : cls->GetMethods()) {
        classDef.methods << QString::fromStdString(method->GetName());
      }

      m_engineClassDefs.push_back(classDef);

      // Debug output
      LogConsole("  Loaded class: " + cls->GetName() + " (" +
                 std::to_string(classDef.members.size()) + " members, " +
                 std::to_string(classDef.methods.size()) + " methods)");
    }
  }

  // Summary
  LogConsole("=== Engine Classes Summary ===");
  LogConsole("Total engine classes: " +
             std::to_string(m_engineClassDefs.size()));
  for (const auto &cls : m_engineClassDefs) {
    LogConsole("  - " + cls.name.toStdString());
  }
}

void ScriptEditorWindow::registerEngineClassesWithEditor(CodeEditor *editor) {
  if (!editor)
    return;

  for (const auto &classDef : m_engineClassDefs) {
    editor->symbolCollector().registerExternalClass(classDef);
  }
}

} // namespace Quantum
