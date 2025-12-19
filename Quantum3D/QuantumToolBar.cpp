#include "QuantumToolBar.h"
#include "EngineGlobals.h"
#include "stdafx.h"
#include <QAction>
#include <QActionGroup>
#include <QtGui/QIcon>
#include <iostream>

QuantumToolBar::QuantumToolBar(QWidget *parent) : QToolBar(parent) {
  setObjectName("MainToolBar");
  setMovable(false);
  setIconSize(QSize(34, 34));
  setupToolBar();
}

QuantumToolBar::~QuantumToolBar() {}

void QuantumToolBar::setupToolBar() {
  // === Coordinate Mode Actions (Local/Global) ===
  m_coordinateActionGroup = new QActionGroup(this);
  m_coordinateActionGroup->setExclusive(true);

  // Local action
  m_localAction = new QAction(this);
  m_localAction->setIcon(QIcon(":/Quantum3D/icons/local.png"));
  m_localAction->setToolTip("Local Coordinates");
  m_localAction->setCheckable(true);
  m_localAction->setChecked(true); // Default selected
  m_coordinateActionGroup->addAction(m_localAction);
  addAction(m_localAction);
  connect(m_localAction, &QAction::triggered, this,
          &QuantumToolBar::onLocalClicked);

  // Global action
  m_globalAction = new QAction(this);
  m_globalAction->setIcon(QIcon(":/Quantum3D/icons/global.png"));
  m_globalAction->setToolTip("Global/World Coordinates");
  m_globalAction->setCheckable(true);
  m_coordinateActionGroup->addAction(m_globalAction);
  addAction(m_globalAction);
  connect(m_globalAction, &QAction::triggered, this,
          &QuantumToolBar::onGlobalClicked);

  // Separator between coordinate mode and gizmo mode
  addSeparator();

  // === Gizmo Mode Actions (Translate/Rotate/Scale) ===
  m_gizmoActionGroup = new QActionGroup(this);
  m_gizmoActionGroup->setExclusive(true);

  // Translate action
  m_translateAction = new QAction(this);
  m_translateAction->setIcon(QIcon(":/Quantum3D/icons/translate.png"));
  m_translateAction->setToolTip("Translate (F1)");
  m_translateAction->setCheckable(true);
  m_translateAction->setChecked(true); // Default selected
  m_translateAction->setShortcut(QKeySequence(Qt::Key_F1));
  m_gizmoActionGroup->addAction(m_translateAction);
  addAction(m_translateAction);
  connect(m_translateAction, &QAction::triggered, this,
          &QuantumToolBar::onTranslateClicked);

  // Rotate action
  m_rotateAction = new QAction(this);
  m_rotateAction->setIcon(QIcon(":/Quantum3D/icons/rotate.png"));
  m_rotateAction->setToolTip("Rotate (F2)");
  m_rotateAction->setCheckable(true);
  m_rotateAction->setShortcut(QKeySequence(Qt::Key_F2));
  m_gizmoActionGroup->addAction(m_rotateAction);
  addAction(m_rotateAction);
  connect(m_rotateAction, &QAction::triggered, this,
          &QuantumToolBar::onRotateClicked);

  // Scale action
  m_scaleAction = new QAction(this);
  m_scaleAction->setIcon(QIcon(":/Quantum3D/icons/scale.png"));
  m_scaleAction->setToolTip("Scale (F3)");
  m_scaleAction->setCheckable(true);
  m_scaleAction->setShortcut(QKeySequence(Qt::Key_F3));
  m_gizmoActionGroup->addAction(m_scaleAction);
  addAction(m_scaleAction);
  connect(m_scaleAction, &QAction::triggered, this,
          &QuantumToolBar::onScaleClicked);

  // Add separator after gizmo tools
  addSeparator();
}

void QuantumToolBar::onLocalClicked() {
  m_currentCoordinateMode = CoordinateMode::Local;
  EngineGlobals::SetSpace(CoordinateSpace::Local);
  emit coordinateModeChanged(m_currentCoordinateMode);
}

void QuantumToolBar::onGlobalClicked() {
  m_currentCoordinateMode = CoordinateMode::Global;
  EngineGlobals::SetSpace(CoordinateSpace::Global);
  emit coordinateModeChanged(m_currentCoordinateMode);
}

void QuantumToolBar::onTranslateClicked() {
  m_currentGizmoMode = GizmoMode::Translate;
  EngineGlobals::SetGizmoMode(GizmoType::Translate);
  emit gizmoModeChanged(m_currentGizmoMode);
}

void QuantumToolBar::onRotateClicked() {
  m_currentGizmoMode = GizmoMode::Rotate;
  EngineGlobals::SetGizmoMode(GizmoType::Rotate);
  emit gizmoModeChanged(m_currentGizmoMode);
}

void QuantumToolBar::onScaleClicked() {
  m_currentGizmoMode = GizmoMode::Scale;
  EngineGlobals::SetGizmoMode(GizmoType::Scale);
  emit gizmoModeChanged(m_currentGizmoMode);
}
