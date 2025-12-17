#include "QuantumToolBar.h"
#include "stdafx.h"
#include <QtGui/QIcon>
#include <QAction>
#include <QActionGroup>
#include <iostream>


QuantumToolBar::QuantumToolBar(QWidget *parent) : QToolBar(parent) {
  setObjectName("MainToolBar");
  setMovable(false);
  setIconSize(QSize(24, 24));
  setupToolBar();
}

QuantumToolBar::~QuantumToolBar() {}

void QuantumToolBar::setupToolBar() {
  // Create action group for exclusive selection
  m_gizmoActionGroup = new QActionGroup(this);
  m_gizmoActionGroup->setExclusive(true);

  // Translate action
  m_translateAction = new QAction(this);
  m_translateAction->setIcon(QIcon(":/Quantum3D/icons/translate.png"));
  m_translateAction->setToolTip("Translate (W)");
  m_translateAction->setCheckable(true);
  m_translateAction->setChecked(true); // Default selected
  m_translateAction->setShortcut(QKeySequence("W"));
  m_gizmoActionGroup->addAction(m_translateAction);
  addAction(m_translateAction);
  connect(m_translateAction, &QAction::triggered, this,
          &QuantumToolBar::onTranslateClicked);

  // Rotate action
  m_rotateAction = new QAction(this);
  m_rotateAction->setIcon(QIcon(":/Quantum3D/icons/rotate.png"));
  m_rotateAction->setToolTip("Rotate (E)");
  m_rotateAction->setCheckable(true);
  m_rotateAction->setShortcut(QKeySequence("E"));
  m_gizmoActionGroup->addAction(m_rotateAction);
  addAction(m_rotateAction);
  connect(m_rotateAction, &QAction::triggered, this,
          &QuantumToolBar::onRotateClicked);

  // Scale action
  m_scaleAction = new QAction(this);
  m_scaleAction->setIcon(QIcon(":/Quantum3D/icons/scale.png"));
  m_scaleAction->setToolTip("Scale (R)");
  m_scaleAction->setCheckable(true);
  m_scaleAction->setShortcut(QKeySequence("R"));
  m_gizmoActionGroup->addAction(m_scaleAction);
  addAction(m_scaleAction);
  connect(m_scaleAction, &QAction::triggered, this,
          &QuantumToolBar::onScaleClicked);

  // Add separator after gizmo tools
  addSeparator();
}

void QuantumToolBar::onTranslateClicked() {
  m_currentGizmoMode = GizmoMode::Translate;
  std::cout << "[QuantumToolBar] Gizmo mode: Translate" << std::endl;
  emit gizmoModeChanged(m_currentGizmoMode);
}

void QuantumToolBar::onRotateClicked() {
  m_currentGizmoMode = GizmoMode::Rotate;
  std::cout << "[QuantumToolBar] Gizmo mode: Rotate" << std::endl;
  emit gizmoModeChanged(m_currentGizmoMode);
}

void QuantumToolBar::onScaleClicked() {
  m_currentGizmoMode = GizmoMode::Scale;
  std::cout << "[QuantumToolBar] Gizmo mode: Scale" << std::endl;
  emit gizmoModeChanged(m_currentGizmoMode);
}
