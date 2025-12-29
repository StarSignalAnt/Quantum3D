#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>

/// Dialog for configuring and running lightmap baking with progress feedback
/// This dialog is self-contained and handles all baking logic internally
class LightmapBakeDialog : public QDialog {
public:
  explicit LightmapBakeDialog(QWidget *parent = nullptr);
  ~LightmapBakeDialog();

  /// Update progress (0.0 - 1.0) and status message
  void UpdateProgress(float progress, const QString &status);

private:
  void CreateUI();
  void StartBaking();
  void OnBakeClicked();
  void OnCloseClicked();

  // Settings controls
  QSpinBox *m_ResolutionSpin;
  QSpinBox *m_ShadowSamplesSpin;
  QSpinBox *m_GIBouncesSpin;
  QSpinBox *m_GISamplesSpin;
  QDoubleSpinBox *m_GIIntensitySpin;
  QCheckBox *m_EnableShadowsCheck;
  QCheckBox *m_EnableGICheck;

  // Progress controls
  QProgressBar *m_ProgressBar;
  QLabel *m_StatusLabel;

  // Buttons
  QPushButton *m_BakeButton;
  QPushButton *m_CloseButton;

  bool m_BakingInProgress = false;
};
