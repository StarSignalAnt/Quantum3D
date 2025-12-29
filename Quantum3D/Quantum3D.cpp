#include "Quantum3D.h"
#include "../../../QLang/QLang/QConsole.h"
#include "../QuantumEngine/SceneGraph.h"
#include "BrowserWidget.h"
#include "ConsoleWidget.h"
#include "EngineGlobals.h"
#include "PropertiesWidget.h"
#include "QLangDomain.h"
#include "QuantumMenu.h"
#include "QuantumToolBar.h"
#include "SceneGraphWidget.h"
#include "ScriptEditorWindow.h"
#include "TerrainEditorWidget.h"
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

  // Start Update Loop (approx 60 FPS)
  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this, &Quantum3D::updateApp);
  m_updateTimer->start(16);
}

Quantum3D::~Quantum3D() {}

void Quantum3D::setupMenu() {
  m_menu = new QuantumMenu(this);
  setMenuBar(m_menu);

  // Connect Terrain Editor toggle
  connect(m_menu->GetTerrainEditorAction(), &QAction::toggled,
          [this](bool visible) {
            if (m_terrainEditorDock) {
              m_terrainEditorDock->setVisible(visible);
            }
          });
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

  // Create Terrain Editor dock widget (in left area with scene graph)
  m_terrainEditorWidget = new TerrainEditorWidget(this);
  m_terrainEditorDock = new QDockWidget(tr("Terrain Editor"), this);
  m_terrainEditorDock->setWidget(m_terrainEditorWidget);
  m_terrainEditorDock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::LeftDockWidgetArea, m_terrainEditorDock);
  tabifyDockWidget(m_sceneGraphDock, m_terrainEditorDock);
  m_sceneGraphDock->raise();   // Scene graph visible by default
  m_terrainEditorDock->hide(); // Hidden until terrain mode
  EngineGlobals::TerrainEditor = m_terrainEditorWidget;

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

  // Connect toolbar mode changes to show/hide terrain editor
  connect(m_toolBar, &QuantumToolBar::editorModeChanged, this,
          [this](Quantum::EditorMode mode) {
            onEditorModeChanged(static_cast<int>(mode));
          });
}

void Quantum3D::onEditorModeChanged(int mode) {
  if (mode == static_cast<int>(Quantum::EditorMode::Terrain)) {
    m_terrainEditorDock->show();
    m_terrainEditorDock->raise();
    std::cout << "[Quantum3D] Switched to Terrain Edit mode" << std::endl;
    EngineGlobals::SetEditorMode(Quantum::EditorMode::Terrain);
    EngineGlobals::ClearSelection(); // Clear existing selection

    // Auto-select terrain
    if (EngineGlobals::EditorScene) {
      auto terrain = EngineGlobals::EditorScene->GetTerrainNode();
      if (terrain) {
        EngineGlobals::SetSelectedNode(terrain);
        std::cout << "[Quantum3D] Auto-selected Terrain: " << terrain->GetName()
                  << std::endl;
      }
    }
  } else {
    m_terrainEditorDock->hide();
    std::cout << "[Quantum3D] Switched to Scene Edit mode" << std::endl;
    EngineGlobals::SetEditorMode(Quantum::EditorMode::Scene);
    // Explicitly select the terrain if valid so user can start editing
    // immediately if coming back? User requested "cannot select another node" -
    // so we force selection to be cleared or just terrain? For now, adhere to
    // "cannot select another node", implies we should probably lock selection
    // to terrain if we have one. The request said "remain the terrain". This
    // implies if we have a terrain selected, keep it.
  }
}

void Quantum3D::updateApp() {
  static float lastTime = 0.0f;
  // Simple dt calculation (placeholder)
  float dt = 0.016f;

  if (EngineGlobals::EditorScene) {
    EngineGlobals::EditorScene->Update(dt);
  }

  // Also request update of viewport if we are animating
  if (m_viewportWidget) {
    m_viewportWidget->update();
  }
}
