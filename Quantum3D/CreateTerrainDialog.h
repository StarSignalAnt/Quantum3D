#pragma once

#include <QDialog>
#include <QSpinBox>

class CreateTerrainDialog : public QDialog {
public:
  explicit CreateTerrainDialog(QWidget *parent = nullptr);
  ~CreateTerrainDialog() = default;

  // Getters for terrain parameters
  float GetWidth() const { return static_cast<float>(m_widthSpin->value()); }
  float GetDepth() const { return static_cast<float>(m_depthSpin->value()); }
  int GetDivisions() const { return m_divisionsSpin->value(); }
  int GetLayers() const { return m_layersSpin->value(); }

private:
  QSpinBox *m_widthSpin;
  QSpinBox *m_depthSpin;
  QSpinBox *m_divisionsSpin;
  QSpinBox *m_layersSpin;
};
