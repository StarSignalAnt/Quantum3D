#include "CreateTerrainDialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

CreateTerrainDialog::CreateTerrainDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Create Terrain"));
  setMinimumWidth(300);

  auto *mainLayout = new QVBoxLayout(this);

  // Title label
  auto *titleLabel = new QLabel(tr("Configure Terrain Parameters"));
  titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
  mainLayout->addWidget(titleLabel);

  // Form layout for inputs
  auto *formLayout = new QFormLayout();

  // Width (X)
  m_widthSpin = new QSpinBox();
  m_widthSpin->setRange(1, 10000);
  m_widthSpin->setValue(100);
  m_widthSpin->setSuffix(" units");
  formLayout->addRow(tr("Width (X):"), m_widthSpin);

  // Depth (Z)
  m_depthSpin = new QSpinBox();
  m_depthSpin->setRange(1, 10000);
  m_depthSpin->setValue(100);
  m_depthSpin->setSuffix(" units");
  formLayout->addRow(tr("Depth (Z):"), m_depthSpin);

  // Divisions
  m_divisionsSpin = new QSpinBox();
  m_divisionsSpin->setRange(1, 1000);
  m_divisionsSpin->setValue(100);
  m_divisionsSpin->setToolTip(
      tr("Number of grid cells. Higher = more detail, more vertices."));
  formLayout->addRow(tr("Divisions:"), m_divisionsSpin);

  // Layers
  m_layersSpin = new QSpinBox();
  m_layersSpin->setRange(1, 4);
  m_layersSpin->setValue(4);
  m_layersSpin->setToolTip(
      tr("Number of texture layers (1-4). Each layer has color, normal, "
         "specular, and blend maps."));
  formLayout->addRow(tr("Layers:"), m_layersSpin);

  mainLayout->addLayout(formLayout);

  // Buttons
  auto *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create"));

  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  mainLayout->addWidget(buttonBox);
}
