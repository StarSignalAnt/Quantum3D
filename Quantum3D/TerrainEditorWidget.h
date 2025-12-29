#pragma once

#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <glm/glm.hpp>
#include <vector>


namespace Quantum {
class TerrainNode;
}

// Terrain texture slot for drag/drop
class TextureSlot : public QFrame {
  Q_OBJECT

public:
  TextureSlot(const QString &label, QWidget *parent = nullptr);
  void SetTexturePath(const QString &path);
  void SetTexturePixmap(const QPixmap &pixmap);
  QString GetTexturePath() const { return m_texturePath; }

signals:
  void textureChanged(const QString &path);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  QString m_label;
  QString m_texturePath;
  QPixmap m_previewPixmap;
};

// Layer group containing color/normal/specular slots
class LayerGroup : public QGroupBox {
  Q_OBJECT

public:
  LayerGroup(int layerIndex, QWidget *parent = nullptr);

  TextureSlot *GetColorSlot() const { return m_colorSlot; }
  TextureSlot *GetNormalSlot() const { return m_normalSlot; }
  TextureSlot *GetSpecularSlot() const { return m_specularSlot; }
  QRadioButton *GetSelectionButton() const { return m_selectionButton; }

signals:
  void textureChanged(int layer, const QString &type, const QString &path);

private:
  int m_layerIndex;
  TextureSlot *m_colorSlot;
  TextureSlot *m_normalSlot;
  TextureSlot *m_specularSlot;
  QRadioButton *m_selectionButton;
};

// Terrain edit mode
enum class TerrainEditMode { Paint, Sculpt };

// Main terrain editor widget
class TerrainEditorWidget : public QWidget {
  Q_OBJECT

public:
  TerrainEditorWidget(QWidget *parent = nullptr);
  virtual ~TerrainEditorWidget();

  void SetTerrain(Quantum::TerrainNode *terrain);
  void Paint(const glm::vec3 &hitPoint);

  TerrainEditMode GetEditMode() const { return m_editMode; }
  int GetSelectedLayer() const { return m_selectedLayer; }
  float GetBrushSize() const { return m_brushSize; }
  float GetBrushStrength() const { return m_brushStrength; }

signals:
  void editModeChanged(TerrainEditMode mode);
  void selectedLayerChanged(int layer);
  void brushSizeChanged(float size);
  void brushStrengthChanged(float strength);
  void layerTextureChanged(int layer, const QString &type, const QString &path);

private slots:
  void onPaintModeClicked();
  void onSculptModeClicked();
  void onBrushSizeChanged(int value);
  void onBrushStrengthChanged(int value);
  void onLayerTextureChanged(int layer, const QString &type,
                             const QString &path);

private:
  void setupUI();
  void rebuildLayers(int layerCount);

  Quantum::TerrainNode *m_terrain = nullptr;

  // Edit mode
  TerrainEditMode m_editMode = TerrainEditMode::Paint;
  QRadioButton *m_paintMode;
  QRadioButton *m_sculptMode;

  // Layers
  QVBoxLayout *m_layersLayout;
  QWidget *m_layersContainer;
  std::vector<LayerGroup *> m_layerGroups;
  QButtonGroup *m_layerSelectionGroup = nullptr;
  int m_selectedLayer = 0;

  // Brush controls
  float m_brushSize = 2.5f;
  float m_brushStrength = 0.5f;
  QSlider *m_sizeSlider;
  QSlider *m_strengthSlider;
  QLabel *m_sizeLabel;
  QLabel *m_strengthLabel;
};
