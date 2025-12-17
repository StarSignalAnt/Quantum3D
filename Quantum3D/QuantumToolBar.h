#pragma once

#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

class QAction;
class QActionGroup;

// Gizmo mode enum
enum class GizmoMode { Translate, Rotate, Scale };

// Coordinate space mode
enum class CoordinateMode { Local, Global };

class QuantumToolBar : public QToolBar {
  Q_OBJECT

public:
  QuantumToolBar(QWidget *parent = nullptr);
  ~QuantumToolBar();

  GizmoMode GetCurrentGizmoMode() const { return m_currentGizmoMode; }
  CoordinateMode GetCurrentCoordinateMode() const {
    return m_currentCoordinateMode;
  }

signals:
  void gizmoModeChanged(GizmoMode mode);
  void coordinateModeChanged(CoordinateMode mode);

private slots:
  void onLocalClicked();
  void onGlobalClicked();
  void onTranslateClicked();
  void onRotateClicked();
  void onScaleClicked();

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
};
