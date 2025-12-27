#include "Quantum3D.h"
#include "../../../QLang/QLang/QConsole.h"
#include "BrowserWidget.h"
#include "ConsoleWidget.h"
#include "EngineGlobals.h"
#include "PropertiesWidget.h"
#include "QLangDomain.h"
#include "QuantumMenu.h"
#include "QuantumToolBar.h"
#include "SceneGraphWidget.h"
#include "ScriptEditorWindow.h"
#include "ViewportWidget.h"
#include "stdafx.h"
#include <QTimer>
#include <iostream>
#include <memory>
#include <variant>
#include <vector>


Quantum3D::Quantum3D(QWidget *parent) : QMainWindow(parent) {
  setupMenu();
  setupToolBar();
  setupDockWidgets();
}

Quantum3D::~Quantum3D() {}

void Quantum3D::setupMenu() {
  m_menu = new QuantumMenu(this);
  setMenuBar(m_menu);
}

void Quantum3D::setupToolBar() {
  m_toolBar = new QuantumToolBar(this);
  addToolBar(m_toolBar);
}

void Quantum3D::setupDockWidgets() {
  // Create Viewport as central widget
  m_viewportWidget = new ViewportWidget(this);
  setCentralWidget(m_viewportWidget);
  EngineGlobals::Viewport = m_viewportWidget;

  // Create Scene Graph dock widget
  m_sceneGraphWidget = new SceneGraphWidget(this);
  m_sceneGraphDock = new QDockWidget(tr("Scene Graph"), this);
  m_sceneGraphDock->setWidget(m_sceneGraphWidget);
  m_sceneGraphDock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::LeftDockWidgetArea, m_sceneGraphDock);
  EngineGlobals::SceneGraphPanel = m_sceneGraphWidget;

  // Create Properties dock widget
  m_propertiesWidget = new PropertiesWidget(this);
  m_propertiesDock = new QDockWidget(tr("Properties"), this);
  m_propertiesDock->setWidget(m_propertiesWidget);
  m_propertiesDock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
  EngineGlobals::PropertiesPanel = m_propertiesWidget;

  // Create Browser dock widget
  m_browserWidget = new BrowserWidget(this);
  m_browserDock = new QDockWidget(tr("Browser"), this);
  m_browserDock->setWidget(m_browserWidget);
  m_browserDock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::BottomDockWidgetArea, m_browserDock);
  EngineGlobals::BrowserPanel = m_browserWidget;

  // Create Console dock widget
  m_consoleWidget = new ConsoleWidget(this);
  m_consoleDock = new QDockWidget(tr("Console"), this);
  m_consoleDock->setWidget(m_consoleWidget);
  m_consoleDock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);
  EngineGlobals::Console = m_consoleWidget;

  // Tabify console with browser so they share the bottom area
  tabifyDockWidget(m_browserDock, m_consoleDock);
  m_browserDock->raise(); // Browser visible by default

  // Connect BrowserWidget model import signal to ViewportWidget slot
  connect(m_browserWidget, &BrowserWidget::ModelImported, m_viewportWidget,
          &ViewportWidget::OnModelImported);

  // Connect BrowserWidget model import to refresh the scene graph widget
  connect(m_browserWidget, &BrowserWidget::ModelImported, m_sceneGraphWidget,
          &SceneGraphWidget::RefreshTree);

  // Set the scene graph on the SceneGraphWidget (will be populated after scene
  // init) Use a timer to defer this until after the viewport has initialized
  // its scene

  EngineGlobals::m_QDomain =
      std::make_shared<QLangDomain>(m_browserWidget->GetCurrentPath());

  // Set initial dock sizes (280 pixels) - resizable by user
  resizeDocks({m_sceneGraphDock}, {280}, Qt::Horizontal);
  resizeDocks({m_propertiesDock}, {280}, Qt::Horizontal);
  resizeDocks({m_browserDock}, {200}, Qt::Vertical);

  // Hook up QLang console delegate to forward output to ConsoleWidget
  // This connects QLang's QConsole to the Qt console widget
  QConsole::SetPrintDelegate(
      [this](const std::string &msg, QConsoleLevel level) {
        if (m_consoleWidget) {
          m_consoleWidget->PrintWithLevel(msg, static_cast<int>(level));
        }
      });

  // Print startup message
  m_consoleWidget->Print("Quantum3D Console initialized.");
}
