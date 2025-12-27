#pragma once

#include "ui_Quantum3D.h"
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QMainWindow>
#include <iostream>

class SceneGraphWidget;
class PropertiesWidget;
class BrowserWidget;
class ViewportWidget;
class QuantumMenu;
class QuantumToolBar;
class ConsoleWidget;

class Quantum3D : public QMainWindow {
  Q_OBJECT

public:
  Quantum3D(QWidget *parent = nullptr);
  ~Quantum3D();

private:
  void setupDockWidgets();
  void setupMenu();
  void setupToolBar();

  Ui::Quantum3DClass ui;

  // Menu bar
  QuantumMenu *m_menu;

  // Tool bar
  QuantumToolBar *m_toolBar;

  // Dock widgets
  QDockWidget *m_sceneGraphDock;
  QDockWidget *m_propertiesDock;
  QDockWidget *m_browserDock;
  QDockWidget *m_consoleDock;

  // Widget contents
  SceneGraphWidget *m_sceneGraphWidget;
  PropertiesWidget *m_propertiesWidget;
  BrowserWidget *m_browserWidget;
  ViewportWidget *m_viewportWidget;
  ConsoleWidget *m_consoleWidget;
};
