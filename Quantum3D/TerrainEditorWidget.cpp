#include "TerrainEditorWidget.h"
#include "EngineGlobals.h"
#include "SceneRenderer.h"
#include "TerrainNode.h"
#include "Texture2D.h"
#include <QButtonGroup>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QPainter>
#include <iostream>

// ============================================================================
// TextureSlot Implementation
// ============================================================================

TextureSlot::TextureSlot(const QString &label, QWidget *parent)
    : QFrame(parent), m_label(label) {
  setAcceptDrops(true);
  setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  setMinimumSize(60, 60);
  setMaximumSize(60, 60);
  setToolTip(label + " (drag texture here)");
}

void TextureSlot::SetTexturePath(const QString &path) {
  m_texturePath = path;
  m_previewPixmap = QPixmap(); // Clear pixmap if path is set
  update();
}

void TextureSlot::SetTexturePixmap(const QPixmap &pixmap) {
  m_previewPixmap = pixmap;
  m_texturePath = ""; // Clear path if pixmap is set
  update();
}

void TextureSlot::dragEnterEvent(QDragEnterEvent *event) {
  // Accept if it has custom texture MIME type or is an image file
  if (event->mimeData()->hasFormat("application/x-texture-image")) {
    event->acceptProposedAction();
    return;
  }

  if (event->mimeData()->hasText()) {
    QString text = event->mimeData()->text();
    if (text.endsWith(".png", Qt::CaseInsensitive) ||
        text.endsWith(".jpg", Qt::CaseInsensitive) ||
        text.endsWith(".jpeg", Qt::CaseInsensitive) ||
        text.endsWith(".bmp", Qt::CaseInsensitive) ||
        text.endsWith(".tga", Qt::CaseInsensitive)) {
      event->acceptProposedAction();
    }
  }
}

void TextureSlot::dropEvent(QDropEvent *event) {
  QString path;

  // Prefer custom MIME type if available
  if (event->mimeData()->hasFormat("application/x-texture-image")) {
    path = QString::fromUtf8(
        event->mimeData()->data("application/x-texture-image"));
  } else if (event->mimeData()->hasText()) {
    path = event->mimeData()->text();
  }

  if (!path.isEmpty()) {
    SetTexturePath(path);
    emit textureChanged(m_texturePath);
    std::cout << "[TextureSlot] Dropped texture: " << path.toStdString()
              << std::endl;
  }
}

void TextureSlot::paintEvent(QPaintEvent *event) {
  QFrame::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QRect r = rect().adjusted(2, 2, -2, -2);

  if (m_texturePath.isEmpty() && m_previewPixmap.isNull()) {
    // Draw placeholder
    painter.setPen(QPen(QColor(100, 100, 100), 1, Qt::DashLine));
    painter.drawRect(r);
    painter.setPen(QColor(150, 150, 150));
    QFont font = painter.font();
    font.setPointSize(7);
    painter.setFont(font);
    painter.drawText(r, Qt::AlignCenter, m_label);
  } else {
    // Draw texture preview
    QPixmap pixmap;
    if (!m_texturePath.isEmpty()) {
      pixmap.load(m_texturePath);
    } else {
      pixmap = m_previewPixmap;
    }

    if (!pixmap.isNull()) {
      painter.drawPixmap(r, pixmap.scaled(r.size(), Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
    } else {
      painter.fillRect(r, QColor(60, 60, 60));
      painter.setPen(Qt::white);
      painter.drawText(r, Qt::AlignCenter, "?");
    }
  }
}

// ============================================================================
// LayerGroup Implementation
// ============================================================================

LayerGroup::LayerGroup(int layerIndex, QWidget *parent)
    : QGroupBox(QString("Layer %1").arg(layerIndex), parent),
      m_layerIndex(layerIndex) {
  // Header layout with radio button
  QHBoxLayout *headerLayout = new QHBoxLayout();
  m_selectionButton = new QRadioButton(this);
  QLabel *titleLabel = new QLabel(QString("Layer %1").arg(layerIndex), this);
  titleLabel->setStyleSheet("font-weight: bold;");

  headerLayout->addWidget(m_selectionButton);
  headerLayout->addWidget(titleLabel);
  headerLayout->addStretch();

  // Replace default group box title with custom header
  setTitle("");
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->addLayout(headerLayout);

  QHBoxLayout *slotsLayout = new QHBoxLayout();
  slotsLayout->setSpacing(5);

  m_colorSlot = new TextureSlot("Color", this);
  m_normalSlot = new TextureSlot("Normal", this);
  m_specularSlot = new TextureSlot("Spec", this);

  slotsLayout->addWidget(m_colorSlot);
  slotsLayout->addWidget(m_normalSlot);
  slotsLayout->addWidget(m_specularSlot);
  slotsLayout->addStretch();

  mainLayout->addLayout(slotsLayout);

  // Remove old direct layout usage since we nested it
  // layout->addStretch();

  // Re-connect signals
  connect(m_colorSlot, &TextureSlot::textureChanged, this,
          [this](const QString &path) {
            emit textureChanged(m_layerIndex, "color", path);
          });
  connect(m_normalSlot, &TextureSlot::textureChanged, this,
          [this](const QString &path) {
            emit textureChanged(m_layerIndex, "normal", path);
          });
  connect(m_specularSlot, &TextureSlot::textureChanged, this,
          [this](const QString &path) {
            emit textureChanged(m_layerIndex, "specular", path);
          });
}

// ============================================================================
// TerrainEditorWidget Implementation
// ============================================================================

TerrainEditorWidget::TerrainEditorWidget(QWidget *parent) : QWidget(parent) {
  setupUI();
}

TerrainEditorWidget::~TerrainEditorWidget() {}

void TerrainEditorWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(10);
  mainLayout->setContentsMargins(10, 10, 10, 10);

  // === Edit Mode Selection ===
  QGroupBox *modeGroup = new QGroupBox("Edit Mode", this);
  QHBoxLayout *modeLayout = new QHBoxLayout(modeGroup);

  m_paintMode = new QRadioButton("Paint", modeGroup);
  m_sculptMode = new QRadioButton("Sculpt", modeGroup);
  m_paintMode->setChecked(true);

  QButtonGroup *modeButtonGroup = new QButtonGroup(this);
  modeButtonGroup->addButton(m_paintMode);
  modeButtonGroup->addButton(m_sculptMode);

  modeLayout->addWidget(m_paintMode);
  modeLayout->addWidget(m_sculptMode);
  modeLayout->addStretch();

  connect(m_paintMode, &QRadioButton::clicked, this,
          &TerrainEditorWidget::onPaintModeClicked);
  connect(m_sculptMode, &QRadioButton::clicked, this,
          &TerrainEditorWidget::onSculptModeClicked);

  mainLayout->addWidget(modeGroup);

  // === Layers Container ===
  m_layersContainer = new QWidget(this);
  m_layersLayout = new QVBoxLayout(m_layersContainer);
  m_layersLayout->setSpacing(5);
  m_layersLayout->setContentsMargins(0, 0, 0, 0);

  // Add placeholder label
  QLabel *noTerrainLabel =
      new QLabel("Select a terrain to edit layers", m_layersContainer);
  noTerrainLabel->setStyleSheet("color: gray;");
  noTerrainLabel->setAlignment(Qt::AlignCenter);
  m_layersLayout->addWidget(noTerrainLabel);

  mainLayout->addWidget(m_layersContainer);

  // === Brush Controls ===
  QGroupBox *brushGroup = new QGroupBox("Brush", this);
  QGridLayout *brushLayout = new QGridLayout(brushGroup);

  // Size
  brushLayout->addWidget(new QLabel("Size:", brushGroup), 0, 0);
  m_sizeSlider = new QSlider(Qt::Horizontal, brushGroup);
  m_sizeSlider->setRange(1, 50); // Mapped to 0.1 - 5.0
  m_sizeSlider->setValue(static_cast<int>(m_brushSize * 10));
  brushLayout->addWidget(m_sizeSlider, 0, 1);
  m_sizeLabel = new QLabel(QString::number(m_brushSize, 'f', 1), brushGroup);
  m_sizeLabel->setMinimumWidth(30);
  brushLayout->addWidget(m_sizeLabel, 0, 2);

  // Strength
  brushLayout->addWidget(new QLabel("Strength:", brushGroup), 1, 0);
  m_strengthSlider = new QSlider(Qt::Horizontal, brushGroup);
  m_strengthSlider->setRange(0, 100);
  m_strengthSlider->setValue(static_cast<int>(m_brushStrength * 100));
  brushLayout->addWidget(m_strengthSlider, 1, 1);
  m_strengthLabel =
      new QLabel(QString::number(m_brushStrength, 'f', 2), brushGroup);
  m_strengthLabel->setMinimumWidth(30);
  brushLayout->addWidget(m_strengthLabel, 1, 2);

  connect(m_sizeSlider, &QSlider::valueChanged, this,
          &TerrainEditorWidget::onBrushSizeChanged);
  connect(m_strengthSlider, &QSlider::valueChanged, this,
          &TerrainEditorWidget::onBrushStrengthChanged);

  mainLayout->addWidget(brushGroup);

  // Spacer
  mainLayout->addStretch();
}

void TerrainEditorWidget::SetTerrain(Quantum::TerrainNode *terrain) {
  m_terrain = terrain;

  if (terrain) {
    rebuildLayers(terrain->GetLayerCount());
    std::cout << "[TerrainEditorWidget] Set terrain with "
              << terrain->GetLayerCount() << " layers" << std::endl;
  } else {
    rebuildLayers(0);
  }
}

void TerrainEditorWidget::rebuildLayers(int layerCount) {
  // Clear existing layers
  for (auto *group : m_layerGroups) {
    m_layersLayout->removeWidget(group);
    delete group;
  }
  m_layerGroups.clear();

  // Clear any remaining widgets (like placeholder label)
  QLayoutItem *item;
  while ((item = m_layersLayout->takeAt(0)) != nullptr) {
    delete item->widget();
    delete item;
  }

  if (layerCount == 0) {
    QLabel *noTerrainLabel =
        new QLabel("Select a terrain to edit layers", m_layersContainer);
    noTerrainLabel->setStyleSheet("color: gray;");
    noTerrainLabel->setAlignment(Qt::AlignCenter);
    m_layersLayout->addWidget(noTerrainLabel);
    return;
  }

  // Create layer groups
  for (int i = 0; i < layerCount; ++i) {
    LayerGroup *group = new LayerGroup(i, m_layersContainer);
    m_layersLayout->addWidget(group);
    m_layerGroups.push_back(group);

    // Populate with current texture previews from terrain
    if (m_terrain) {
      const auto &layer = m_terrain->GetLayer(i);
      auto updateSlot = [&](TextureSlot *slot,
                            const std::shared_ptr<Vivid::Texture2D> &tex,
                            const std::string &path, const std::string &type) {
        if (!path.empty()) {
          slot->SetTexturePath(QString::fromStdString(path));
        } else if (tex) {
          // Read pixels from GPU
          auto pixels = tex->GetPixels();
          if (!pixels.empty()) {
            // Create QImage that owns its data
            QImage img(tex->GetWidth(), tex->GetHeight(),
                       QImage::Format_RGBA8888);
            if (img.sizeInBytes() == pixels.size()) {
              std::memcpy(img.bits(), pixels.data(), pixels.size());
              slot->SetTexturePixmap(QPixmap::fromImage(img));
              std::cout << "[TerrainEditor] Set preview from pixels for "
                        << type << " size: " << pixels.size() << std::endl;
            } else {
              std::cout << "[TerrainEditor] Mismatch size for " << type
                        << " Img: " << img.sizeInBytes()
                        << " Pixels: " << pixels.size() << std::endl;
            }
          } else {
            std::cout << "[TerrainEditor] Empty pixels for " << type
                      << std::endl;
          }
        } else {
          std::cout << "[TerrainEditor] No texture for " << type << std::endl;
        }
      };

      updateSlot(group->GetColorSlot(), layer.colorMap, layer.colorPath,
                 "Color");
      updateSlot(group->GetNormalSlot(), layer.normalMap, layer.normalPath,
                 "Normal");
      updateSlot(group->GetSpecularSlot(), layer.specularMap,
                 layer.specularPath, "Specular");
    }

    connect(group, &LayerGroup::textureChanged, this,
            &TerrainEditorWidget::onLayerTextureChanged);

    // Add to selection group
    if (!m_layerSelectionGroup) {
      m_layerSelectionGroup = new QButtonGroup(this);
      m_layerSelectionGroup->setExclusive(true);
      connect(m_layerSelectionGroup,
              QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
              [this](QAbstractButton *button) {
                for (int i = 0; i < m_layerGroups.size(); ++i) {
                  if (m_layerGroups[i]->GetSelectionButton() == button) {
                    m_selectedLayer = i;
                    emit selectedLayerChanged(i);
                    std::cout << "Selected Paint Layer: " << i << std::endl;
                    break;
                  }
                }
              });
    }
    m_layerSelectionGroup->addButton(group->GetSelectionButton());

    // Select first layer by default
    if (i == 0) {
      group->GetSelectionButton()->setChecked(true);
      m_selectedLayer = 0;
    }
  }
}

void TerrainEditorWidget::Paint(const glm::vec3 &hitPoint) {
  if (!m_terrain)
    return;

  if (m_editMode == TerrainEditMode::Paint) {
    m_terrain->Paint(hitPoint, m_selectedLayer, m_brushSize,
                     m_brushStrength * 0.1f);
  } else if (m_editMode == TerrainEditMode::Sculpt) {
    float strength = m_brushStrength;

    // Check for Shift key to invert strength (lower terrain)
    if (QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier) {
      strength = -strength;
    }

    // Arbitrary scaling for sculpt speed
    strength *= 2.0f;

    m_terrain->Sculpt(hitPoint, m_brushSize, strength);

    // Update terrain gizmo to conform to new terrain shape
    if (EngineGlobals::Renderer) {
      EngineGlobals::Renderer->UpdateTerrainGizmo();
    }
  }
}

void TerrainEditorWidget::onPaintModeClicked() {
  m_editMode = TerrainEditMode::Paint;
  emit editModeChanged(m_editMode);
}

void TerrainEditorWidget::onSculptModeClicked() {
  m_editMode = TerrainEditMode::Sculpt;
  emit editModeChanged(m_editMode);
}

void TerrainEditorWidget::onBrushSizeChanged(int value) {
  m_brushSize = value / 10.0f;
  m_sizeLabel->setText(QString::number(m_brushSize, 'f', 1));
  emit brushSizeChanged(m_brushSize);
}

void TerrainEditorWidget::onBrushStrengthChanged(int value) {
  m_brushStrength = value / 100.0f;
  m_strengthLabel->setText(QString::number(m_brushStrength, 'f', 2));
  emit brushStrengthChanged(m_brushStrength);
}

void TerrainEditorWidget::onLayerTextureChanged(int layer, const QString &type,
                                                const QString &path) {
  if (!m_terrain) {
    return;
  }

  std::cout << "[TerrainEditorWidget] Layer " << layer << " "
            << type.toStdString()
            << " texture changed to: " << path.toStdString() << std::endl;

  // Update the terrain with the new texture
  m_terrain->SetLayerTexture(layer, type.toStdString(), path.toStdString());

  // Emit signal for any external listeners
  emit layerTextureChanged(layer, type, path);
}
