#include "QuantumToolBar.h"
#include "stdafx.h"


QuantumToolBar::QuantumToolBar(QWidget *parent) : QToolBar(parent) {
  setObjectName("MainToolBar");
  setMovable(false);
  setupToolBar();
}

QuantumToolBar::~QuantumToolBar() {}

void QuantumToolBar::setupToolBar() {
  // Empty - add buttons here as needed
}
