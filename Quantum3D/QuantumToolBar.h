#pragma once

#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

class QAction;
class QActionGroup;

// Gizmo mode enum
enum class GizmoMode { Translate, Rotate, Scale };

class QuantumToolBar : public QToolBar {
  Q_OBJECT

public:
  QuantumToolBar(QWidget *parent = nullptr);
  ~QuantumToolBar();

  GizmoMode GetCurrentGizmoMode() const { return m_currentGizmoMode; }

signals:
  void gizmoModeChanged(GizmoMode mode);

private slots:
  void onTranslateClicked();
  void onRotateClicked();
  void onScaleClicked();

private:
  void setupToolBar();

  QAction *m_translateAction;
  QAction *m_rotateAction;
  QAction *m_scaleAction;
  QActionGroup *m_gizmoActionGroup;

  GizmoMode m_currentGizmoMode = GizmoMode::Translate;
};
