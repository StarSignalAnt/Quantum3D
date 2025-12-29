#pragma once

#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

class QAction;
class QActionGroup;
class QComboBox;

// Gizmo mode enum
enum class GizmoMode { Translate, Rotate, Scale };

// Coordinate space mode
enum class CoordinateMode { Local, Global };

// Editor mode enum
#include "EditorCommon.h"

class QuantumToolBar : public QToolBar {
  Q_OBJECT

public:
  QuantumToolBar(QWidget *parent = nullptr);
  ~QuantumToolBar();

  GizmoMode GetCurrentGizmoMode() const { return m_currentGizmoMode; }
  CoordinateMode GetCurrentCoordinateMode() const {
    return m_currentCoordinateMode;
  }
  Quantum::EditorMode GetCurrentEditorMode() const {
    return m_currentEditorMode;
  }

signals:
  void gizmoModeChanged(GizmoMode mode);
  void coordinateModeChanged(CoordinateMode mode);
  void playStateChanged(bool isPlaying);
  void editorModeChanged(Quantum::EditorMode mode);

private slots:
  void onLocalClicked();
  void onGlobalClicked();
  void onTranslateClicked();
  void onRotateClicked();
  void onScaleClicked();
  void onPlayClicked();
  void onStopClicked();
  void onEditorModeChanged(int index);

private:
  void setupToolBar();

  // Coordinate mode actions
  QAction *m_localAction;
  QAction *m_globalAction;
  QActionGroup *m_coordinateActionGroup;

  // Gizmo mode actions
  QAction *m_translateAction;
  QAction *m_rotateAction;
  QAction *m_scaleAction;
  QActionGroup *m_gizmoActionGroup;

  GizmoMode m_currentGizmoMode = GizmoMode::Translate;
  CoordinateMode m_currentCoordinateMode = CoordinateMode::Local;
  bool m_isPlaying = false;

  // Play control actions
  QAction *m_playAction;
  QAction *m_stopAction;

  // Editor mode selector
  QComboBox *m_editorModeCombo;
  Quantum::EditorMode m_currentEditorMode = Quantum::EditorMode::Scene;
};
