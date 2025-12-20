#include "QuantumMenu.h"
#include "EngineGlobals.h"
#include "ScriptEditorWindow.h"
#include "stdafx.h"


QuantumMenu::QuantumMenu(QWidget *parent) : QMenuBar(parent) { setupMenus(); }

QuantumMenu::~QuantumMenu() {}

void QuantumMenu::setupMenus() {
  // File Menu
  m_fileMenu = addMenu(tr("&File"));

  // Edit Menu
  m_editMenu = addMenu(tr("&Edit"));

  // View Menu
  m_viewMenu = addMenu(tr("&View"));

  // Tools Menu
  m_toolsMenu = addMenu(tr("&Tools"));
  m_scriptEditorAction = m_toolsMenu->addAction(tr("&Script Editor"));

  // Connect via EngineGlobals
  connect(m_scriptEditorAction, &QAction::triggered, []() {
    if (EngineGlobals::ScriptEditor) {
      EngineGlobals::ScriptEditor->show();
      EngineGlobals::ScriptEditor->raise();
      EngineGlobals::ScriptEditor->activateWindow();
    }
  });

  // Help Menu
  m_helpMenu = addMenu(tr("&Help"));
}
