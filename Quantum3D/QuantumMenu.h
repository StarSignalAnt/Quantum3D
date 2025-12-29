#pragma once

#include <QtWidgets/QMenuBar>

class QuantumMenu : public QMenuBar {
  Q_OBJECT

public:
  QuantumMenu(QWidget *parent = nullptr);
  ~QuantumMenu();

  QAction *GetTerrainEditorAction() const { return m_terrainEditorAction; }

private:
  void setupMenus();

  QMenu *m_fileMenu;
  QMenu *m_editMenu;
  QMenu *m_viewMenu;
  QMenu *m_toolsMenu;
  QMenu *m_renderingMenu; // Lightmap baking menu
  QMenu *m_helpMenu;

  QAction *m_openSceneAction;
  QAction *m_newSceneAction; // Added
  QAction *m_saveSceneAction;
  QAction *m_copyAction;           // Added
  QAction *m_pasteAction;          // Added
  QAction *m_alignNodeToCamAction; // Added
  QAction *m_alignCamToNodeAction; // Added
  QAction *m_terrainEditorAction;  // Added
  QAction *m_scriptEditorAction;
  QAction *m_bakeLightmapsAction; // Lightmap baking

  QMenu *m_createMenu;
  QAction *m_createWaterAction;
  QAction *m_createTerrainAction;
};
