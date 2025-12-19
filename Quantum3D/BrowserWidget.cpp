#include "BrowserWidget.h"
#include "stdafx.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileIconProvider>
#include <QtWidgets/QVBoxLayout>
#include <algorithm>
#include <filesystem>

#include "../QuantumEngine/ModelImporter.h"
#include "../QuantumEngine/SceneGraph.h"
#include "EngineGlobals.h"

// =============================================================================
// ThumbnailCache Implementation
// =============================================================================

ThumbnailCache &ThumbnailCache::Instance() {
  static ThumbnailCache instance;
  return instance;
}

QIcon ThumbnailCache::GetThumbnail(const std::string &path, int size) {
  // Check cache first
  auto it = m_Cache.find(path);
  if (it != m_Cache.end()) {
    return it->second.icon;
  }

  // Load image and create thumbnail
  QPixmap pixmap(QString::fromStdString(path));
  if (pixmap.isNull()) {
    return QIcon(); // Return empty icon if failed to load
  }

  // Scale to thumbnail size, keeping aspect ratio
  QPixmap scaled =
      pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // Estimate memory usage (rough: width * height * 4 bytes per pixel)
  size_t entrySize = scaled.width() * scaled.height() * 4;

  // Evict old entries if needed
  EvictIfNeeded(entrySize);

  // Create icon and cache it
  QIcon icon(scaled);
  CacheEntry entry{icon, entrySize};
  m_Cache[path] = entry;
  m_CurrentSizeBytes += entrySize;

  return icon;
}

void ThumbnailCache::Clear() {
  m_Cache.clear();
  m_CurrentSizeBytes = 0;
}

void ThumbnailCache::EvictIfNeeded(size_t newEntrySize) {
  // If adding new entry would exceed limit, evict oldest entries
  while (m_CurrentSizeBytes + newEntrySize > m_MemoryLimitBytes &&
         !m_Cache.empty()) {
    // Simple eviction: remove first entry (not LRU, but simple)
    auto it = m_Cache.begin();
    m_CurrentSizeBytes -= it->second.sizeBytes;
    m_Cache.erase(it);
  }
}

// =============================================================================
// BrowserItemDelegate Implementation
// =============================================================================

BrowserItemDelegate::BrowserItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void BrowserItemDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  painter->save();

  // Get item data
  QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
  QString text = index.data(Qt::DisplayRole).toString();

  // Calculate icon rect (centered at top)
  int iconSize = option.decorationSize.width();
  int iconX = option.rect.x() + (option.rect.width() - iconSize) / 2;
  int iconY = option.rect.y() + 4;
  QRect iconRect(iconX, iconY, iconSize, iconSize);

  // Calculate text rect (below icon)
  int textY = iconY + iconSize + 4;
  int textHeight = option.rect.bottom() - textY;
  QRect textRect(option.rect.x(), textY, option.rect.width(), textHeight);

  // Draw the icon
  icon.paint(painter, iconRect);

  // Draw elided text for ALL items (hovered overlay is drawn separately LAST)
  painter->setPen(QColor(220, 220, 220));
  QFontMetrics fm(option.font);
  QString elidedText =
      fm.elidedText(text, Qt::ElideMiddle, textRect.width() - 4);
  painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, elidedText);

  painter->restore();
}

QSize BrowserItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);
  return QSize(102, 122);
}

// =============================================================================
// BrowserListWidget Implementation
// =============================================================================

BrowserListWidget::BrowserListWidget(QWidget *parent)
    : QListWidget(parent), m_HoveredIndex(-1) {}

void BrowserListWidget::setHoveredIndex(int index) {
  if (m_HoveredIndex != index) {
    m_HoveredIndex = index;
    viewport()->update();
  }
}

void BrowserListWidget::paintEvent(QPaintEvent *event) {
  // First, let Qt paint all items normally
  QListWidget::paintEvent(event);

  // Then draw the hovered item overlay LAST (on top of everything)
  if (m_HoveredIndex >= 0 && m_HoveredIndex < count()) {
    QPainter painter(viewport());
    drawHoveredOverlay(&painter);
  }
}

void BrowserListWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_DragStartPosition = event->pos();
  }
  QListWidget::mousePressEvent(event);
}

void BrowserListWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton)) {
    QListWidget::mouseMoveEvent(event);
    return;
  }

  // Check if we've moved far enough to start a drag
  if ((event->pos() - m_DragStartPosition).manhattanLength() <
      QApplication::startDragDistance()) {
    QListWidget::mouseMoveEvent(event);
    return;
  }

  // Get the item at the drag start position
  QListWidgetItem *dragItem = itemAt(m_DragStartPosition);
  if (dragItem) {
    startDrag(dragItem);
  }
}

void BrowserListWidget::startDrag(QListWidgetItem *item) {
  QString filePath = item->data(Qt::UserRole).toString();

  // Only allow dragging .q files
  if (!filePath.endsWith(".q", Qt::CaseInsensitive)) {
    return;
  }

  QMimeData *mimeData = new QMimeData();
  mimeData->setData("application/x-qlang-script", filePath.toUtf8());
  mimeData->setText(filePath);

  QDrag *drag = new QDrag(this);
  drag->setMimeData(mimeData);

  // Set a visual representation for the drag
  QPixmap pixmap = item->icon().pixmap(48, 48);
  drag->setPixmap(pixmap);
  drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));

  drag->exec(Qt::CopyAction);
}

void BrowserListWidget::drawHoveredOverlay(QPainter *painter) {
  QListWidgetItem *hoveredItem = item(m_HoveredIndex);
  if (!hoveredItem)
    return;

  QRect itemRect = visualItemRect(hoveredItem);

  // Calculate positions - use QListWidget::iconSize() method
  int iconSz = QListWidget::iconSize().width();
  if (iconSz == 0)
    iconSz = 77; // fallback
  int iconY = itemRect.y() + 4;
  int textY = iconY + iconSz + 4;

  // Get full filename
  QString text = hoveredItem->text();
  QFont font = this->font();
  QFontMetrics fm(font);

  // Calculate text background size
  int textWidth = fm.horizontalAdvance(text);
  int padding = 6;
  int bgWidth = textWidth + padding * 2;
  int bgX = itemRect.x() + (itemRect.width() - bgWidth) / 2;
  int bgHeight = fm.height() + 4;

  // Clamp to viewport bounds so it doesn't render outside the widget
  int viewportWidth = viewport()->width();
  if (bgX < 0) {
    bgX = 0; // Clamp to left edge
  }
  if (bgX + bgWidth > viewportWidth) {
    bgX = viewportWidth - bgWidth; // Clamp to right edge
    if (bgX < 0) {
      bgX = 0;                 // If still negative, just start at 0
      bgWidth = viewportWidth; // And limit width to viewport
    }
  }

  QRect bgRect(bgX, textY, bgWidth, bgHeight);

  // Draw highlight on icon
  int iconX = itemRect.x() + (itemRect.width() - iconSz) / 2;
  QRect iconRect(iconX, iconY, iconSz, iconSz);
  painter->fillRect(iconRect.adjusted(-4, -4, 4, 4),
                    QColor(100, 100, 100, 100));

  // Redraw icon on top of highlight
  QIcon icon = hoveredItem->icon();
  icon.paint(painter, iconRect);

  // Draw dark grey background for text with white outline
  painter->fillRect(bgRect, QColor(50, 50, 55));
  painter->setPen(QPen(QColor(255, 255, 255), 1));
  painter->drawRect(bgRect);

  // Draw full text
  painter->setPen(QColor(220, 220, 220));
  painter->setFont(font);
  painter->drawText(bgRect, Qt::AlignCenter, text);
}

// =============================================================================
// BrowserWidget Implementation
// =============================================================================

bool BrowserWidget::IsImageFile(const std::string &extension) {
  return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
         extension == ".bmp" || extension == ".gif" || extension == ".tga" ||
         extension == ".tiff" || extension == ".webp";
}

BrowserWidget::BrowserWidget(QWidget *parent) : QWidget(parent) {
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_ListWidget = new BrowserListWidget(this);
  // Style only the viewport, not the scrollbars
  m_ListWidget->setStyleSheet(
      "QListWidget { padding: 8px; }"
      "QScrollBar:vertical { width: 14px; }"
      "QScrollBar::handle:vertical { min-height: 30px; }");
  m_ListWidget->setViewMode(QListWidget::IconMode);
  m_ListWidget->setResizeMode(QListWidget::Adjust);

  int iconSize = 77;
  int gridWidth = 102;
  int gridHeight = 122;

  m_ListWidget->setGridSize(QSize(gridWidth, gridHeight));
  m_ListWidget->setIconSize(QSize(iconSize, iconSize));
  m_ListWidget->setSpacing(5);
  m_ListWidget->setWordWrap(true);
  m_ListWidget->setUniformItemSizes(true);
  m_ListWidget->setMouseTracking(true);

  // Ensure scrollbars work properly
  m_ListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_ListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_ListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_ListWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

  m_ItemDelegate = new BrowserItemDelegate(this);
  m_ListWidget->setItemDelegate(m_ItemDelegate);

  layout->addWidget(m_ListWidget);

  // Load custom icons
  m_FolderIcon = QIcon("engine/icons/folder_icon.png");
  m_FileIcon = QIcon("engine/icons/file_icon.png");
  m_ModelIcon = QIcon("engine/icons/model_icon.png");
  m_ScriptIcon = QIcon("engine/icons/script_icon.png");
  m_ImageIcon = m_FileIcon; // Use file icon as fallback for failed thumbnails

  m_ListWidget->installEventFilter(this);
  m_ListWidget->viewport()->installEventFilter(this);
  connect(m_ListWidget, &QListWidget::itemDoubleClicked, this,
          &BrowserWidget::OnItemDoubleClicked);

  BrowsePath("c:\\qcontent\\");
}

BrowserWidget::~BrowserWidget() {
  // Clear thumbnail cache on exit
  ThumbnailCache::Instance().Clear();
}

void BrowserWidget::BrowsePath(const std::string &path) {
  m_CurrentPath = path;
  m_ListWidget->clear();

  if (!std::filesystem::exists(path)) {
    std::filesystem::create_directories(path);
  }

  std::vector<std::filesystem::directory_entry> files;
  std::vector<std::filesystem::directory_entry> folders;

  try {
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
      if (entry.is_directory()) {
        folders.push_back(entry);
      } else {
        files.push_back(entry);
      }
    }
  } catch (const std::filesystem::filesystem_error &e) {
    return;
  }

  int gridWidth = 102;
  int gridHeight = 122;
  int iconSize = 77;

  auto addItem = [&](const std::filesystem::directory_entry &entry,
                     bool isFolder) {
    QString filename = QString::fromStdString(entry.path().filename().string());
    QIcon icon;

    if (isFolder) {
      icon = m_FolderIcon;
    } else {
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

      if (IsImageFile(ext)) {
        // Try to load thumbnail from cache
        std::string fullPath = entry.path().string();
        icon = ThumbnailCache::Instance().GetThumbnail(fullPath, iconSize);
        if (icon.isNull()) {
          icon = m_ImageIcon; // Fallback if thumbnail failed
        }
      } else if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
                 ext == ".glb") {
        icon = m_ModelIcon;
      } else if (ext == ".q" || ext == ".lua" || ext == ".py" || ext == ".js" ||
                 ext == ".cpp" || ext == ".h") {
        icon = m_ScriptIcon;
      } else {
        icon = m_FileIcon;
      }
    }

    QListWidgetItem *item = new QListWidgetItem(icon, filename);
    item->setSizeHint(QSize(gridWidth, gridHeight));
    item->setTextAlignment(Qt::AlignHCenter);
    item->setData(Qt::UserRole, QString::fromStdString(entry.path().string()));
    m_ListWidget->addItem(item);
  };

  for (const auto &folder : folders) {
    addItem(folder, true);
  }

  for (const auto &file : files) {
    addItem(file, false);
  }
}

void BrowserWidget::OnItemDoubleClicked(QListWidgetItem *item) {
  std::string path = item->data(Qt::UserRole).toString().toStdString();
  if (std::filesystem::is_directory(path)) {
    m_History.push(m_CurrentPath);
    BrowsePath(path);
  } else {
    std::string extension = std::filesystem::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   ::tolower);

    if (extension == ".fbx" || extension == ".obj" || extension == ".gltf" ||
        extension == ".glb") {
      if (EngineGlobals::EditorScene && EngineGlobals::VulkanDevice) {
        auto model = Quantum::ModelImporter::ImportEntity(
            path,
            static_cast<Vivid::VividDevice *>(EngineGlobals::VulkanDevice));
        if (model) {
          EngineGlobals::EditorScene->GetRoot()->AddChild(model);
          qDebug() << "Imported model to scene: " << path.c_str();
          emit ModelImported();
        }
      }
    }
  }
}

bool BrowserWidget::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_ListWidget->viewport()) {
    if (event->type() == QEvent::MouseMove) {
      QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
      QModelIndex index = m_ListWidget->indexAt(mouseEvent->pos());
      m_ListWidget->setHoveredIndex(index.isValid() ? index.row() : -1);
    } else if (event->type() == QEvent::Leave) {
      m_ListWidget->setHoveredIndex(-1);
    }
  }

  if ((watched == m_ListWidget || watched == m_ListWidget->viewport()) &&
      event->type() == QEvent::MouseButtonPress) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::XButton1) {
      if (!m_History.empty()) {
        std::string previousPath = m_History.top();
        m_History.pop();
        BrowsePath(previousPath);
        return true;
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}
