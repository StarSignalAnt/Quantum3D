#include "QuantumMenu.h"
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

  // Help Menu
  m_helpMenu = addMenu(tr("&Help"));
}
