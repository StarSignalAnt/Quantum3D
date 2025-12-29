#include "LightmapBakeDialog.h"
#include "../QuantumEngine/LightmapBaker.h"
#include "../QuantumEngine/SceneRenderer.h"
#include "EngineGlobals.h"
#include "ViewportWidget.h"
#include <QtCore/QCoreApplication>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>

LightmapBakeDialog::LightmapBakeDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Bake Lightmaps");
  setMinimumWidth(400);
  setModal(false); // Allow interaction with main window
  CreateUI();
}

LightmapBakeDialog::~LightmapBakeDialog() {}

void LightmapBakeDialog::CreateUI() {
  auto *mainLayout = new QVBoxLayout(this);

  // Settings Group
  auto *settingsGroup = new QGroupBox("Bake Settings", this);
  auto *formLayout = new QFormLayout(settingsGroup);

  // Resolution
  m_ResolutionSpin = new QSpinBox(this);
  m_ResolutionSpin->setRange(32, 2048);
  m_ResolutionSpin->setValue(256);
  m_ResolutionSpin->setSingleStep(32);
  formLayout->addRow("Resolution:", m_ResolutionSpin);

  // Shadow Samples
  m_ShadowSamplesSpin = new QSpinBox(this);
  m_ShadowSamplesSpin->setRange(1, 64);
  m_ShadowSamplesSpin->setValue(16);
  formLayout->addRow("Shadow Samples:", m_ShadowSamplesSpin);

  // Enable Shadows
  m_EnableShadowsCheck = new QCheckBox(this);
  m_EnableShadowsCheck->setChecked(true);
  formLayout->addRow("Enable Shadows:", m_EnableShadowsCheck);

  // GI Bounces
  m_GIBouncesSpin = new QSpinBox(this);
  m_GIBouncesSpin->setRange(0, 8);
  m_GIBouncesSpin->setValue(2);
  formLayout->addRow("GI Bounces:", m_GIBouncesSpin);

  // GI Samples
  m_GISamplesSpin = new QSpinBox(this);
  m_GISamplesSpin->setRange(8, 256);
  m_GISamplesSpin->setValue(32);
  formLayout->addRow("GI Samples:", m_GISamplesSpin);

  // GI Intensity
  m_GIIntensitySpin = new QDoubleSpinBox(this);
  m_GIIntensitySpin->setRange(0.0, 5.0);
  m_GIIntensitySpin->setValue(1.0);
  m_GIIntensitySpin->setSingleStep(0.1);
  formLayout->addRow("GI Intensity:", m_GIIntensitySpin);

  // Enable GI
  m_EnableGICheck = new QCheckBox(this);
  m_EnableGICheck->setChecked(true);
  formLayout->addRow("Enable GI:", m_EnableGICheck);

  settingsGroup->setLayout(formLayout);
  mainLayout->addWidget(settingsGroup);

  // Progress Group
  auto *progressGroup = new QGroupBox("Progress", this);
  auto *progressLayout = new QVBoxLayout(progressGroup);

  m_StatusLabel = new QLabel("Ready to bake", this);
  progressLayout->addWidget(m_StatusLabel);

  m_ProgressBar = new QProgressBar(this);
  m_ProgressBar->setRange(0, 100);
  m_ProgressBar->setValue(0);
  progressLayout->addWidget(m_ProgressBar);

  progressGroup->setLayout(progressLayout);
  mainLayout->addWidget(progressGroup);

  // Buttons
  auto *buttonLayout = new QHBoxLayout();

  m_BakeButton = new QPushButton("Bake", this);
  m_BakeButton->setDefault(true);
  connect(m_BakeButton, &QPushButton::clicked, this,
          &LightmapBakeDialog::OnBakeClicked);
  buttonLayout->addWidget(m_BakeButton);

  m_CloseButton = new QPushButton("Close", this);
  connect(m_CloseButton, &QPushButton::clicked, this,
          &LightmapBakeDialog::OnCloseClicked);
  buttonLayout->addWidget(m_CloseButton);

  mainLayout->addLayout(buttonLayout);

  setLayout(mainLayout);
}

void LightmapBakeDialog::UpdateProgress(float progress, const QString &status) {
  m_ProgressBar->setValue(static_cast<int>(progress * 100.0f));
  m_StatusLabel->setText(status);

  // Process events to update UI during baking
  QCoreApplication::processEvents();
}

void LightmapBakeDialog::OnBakeClicked() { StartBaking(); }

void LightmapBakeDialog::OnCloseClicked() {
  if (!m_BakingInProgress) {
    close();
  }
}

void LightmapBakeDialog::StartBaking() {
  // Check viewport availability
  if (!EngineGlobals::Viewport) {
    QMessageBox::warning(this, "Error", "No viewport available.");
    return;
  }

  auto *renderer = EngineGlobals::Viewport->GetSceneRenderer();
  if (!renderer) {
    QMessageBox::warning(this, "Error", "No scene renderer available.");
    return;
  }

  // Build settings from UI controls
  Quantum::BakeSettings settings;
  settings.resolution = m_ResolutionSpin->value();
  settings.shadowSamples = m_ShadowSamplesSpin->value();
  settings.giBounces = m_GIBouncesSpin->value();
  settings.giSamples = m_GISamplesSpin->value();
  settings.giIntensity = static_cast<float>(m_GIIntensitySpin->value());
  settings.enableShadows = m_EnableShadowsCheck->isChecked();
  settings.enableGI = m_EnableGICheck->isChecked();

  // Disable controls during baking
  m_BakingInProgress = true;
  m_ResolutionSpin->setEnabled(false);
  m_ShadowSamplesSpin->setEnabled(false);
  m_GIBouncesSpin->setEnabled(false);
  m_GISamplesSpin->setEnabled(false);
  m_GIIntensitySpin->setEnabled(false);
  m_EnableShadowsCheck->setEnabled(false);
  m_EnableGICheck->setEnabled(false);
  m_BakeButton->setEnabled(false);
  m_BakeButton->setText("Baking...");

  UpdateProgress(0.0f, "Starting lightmap bake...");

  // Capture 'this' for the lambda callback
  LightmapBakeDialog *dialog = this;

  // Run baking with progress callback
  bool success = renderer->BakeLightmaps(
      settings, [dialog](float progress, const std::string &status) {
        dialog->UpdateProgress(progress, QString::fromStdString(status));
      });

  // Re-enable controls
  m_BakingInProgress = false;
  m_ResolutionSpin->setEnabled(true);
  m_ShadowSamplesSpin->setEnabled(true);
  m_GIBouncesSpin->setEnabled(true);
  m_GISamplesSpin->setEnabled(true);
  m_GIIntensitySpin->setEnabled(true);
  m_EnableShadowsCheck->setEnabled(true);
  m_EnableGICheck->setEnabled(true);
  m_BakeButton->setEnabled(true);
  m_BakeButton->setText("Bake");

  if (success) {
    // Refresh material descriptors so they include the new lightmap textures
    renderer->RefreshMaterialTextures();

    UpdateProgress(1.0f, "Baking complete!");
    QMessageBox::information(this, "Lightmap Baking",
                             "Lightmap baking completed successfully!");
  } else {
    UpdateProgress(0.0f, "Baking failed.");
    QMessageBox::warning(
        this, "Lightmap Baking",
        QString::fromStdString("Lightmap baking failed: " +
                               renderer->GetLightmapBaker().GetLastError()));
  }
}
