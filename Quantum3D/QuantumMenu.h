#pragma once

#include <QtWidgets/QMenuBar>

class QuantumMenu : public QMenuBar {
  Q_OBJECT

public:
  QuantumMenu(QWidget *parent = nullptr);
  ~QuantumMenu();

private:
  void setupMenus();

  QMenu *m_fileMenu;
  QMenu *m_editMenu;
  QMenu *m_viewMenu;
  QMenu *m_toolsMenu;
  QMenu *m_helpMenu;

  QAction *m_scriptEditorAction;
};
