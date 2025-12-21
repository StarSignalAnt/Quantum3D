#pragma once

#include <QtCore/QMimeData>
#include <QtGui/QDrag>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QWidget>
#include <stack>
#include <string>
#include <unordered_map>

class QListWidgetItem;
class QMouseEvent;

// Custom delegate that renders items normally
class BrowserItemDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit BrowserItemDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
};

// Custom list widget that defers rendering of hovered item overlay
class BrowserListWidget : public QListWidget {
  Q_OBJECT
public:
  explicit BrowserListWidget(QWidget *parent = nullptr);

  void setHoveredIndex(int index);
  int hoveredIndex() const { return m_HoveredIndex; }

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  void drawHoveredOverlay(QPainter *painter);
  void startDrag(QListWidgetItem *item);

  int m_HoveredIndex = -1;
  QPoint m_DragStartPosition;
};

// Simple thumbnail cache with memory limit
class ThumbnailCache {
public:
  static ThumbnailCache &Instance();

  // Get cached thumbnail or load and cache it
  QIcon GetThumbnail(const std::string &path, int size);

  // Clear entire cache
  void Clear();

  // Set memory limit in MB (default 50MB)
  void SetMemoryLimitMB(size_t mb) { m_MemoryLimitBytes = mb * 1024 * 1024; }

private:
  ThumbnailCache() = default;

  struct CacheEntry {
    QIcon icon;
    size_t sizeBytes;
  };

  std::unordered_map<std::string, CacheEntry> m_Cache;
  size_t m_CurrentSizeBytes = 0;
  size_t m_MemoryLimitBytes = 50 * 1024 * 1024; // 50MB default

  void EvictIfNeeded(size_t newEntrySize);
};

class BrowserWidget : public QWidget {
  Q_OBJECT

public:
  BrowserWidget(QWidget *parent = nullptr);
  ~BrowserWidget();

  void BrowsePath(const std::string &path);
  std::string GetCurrentPath() const { return m_CurrentPath; }
  std::string GetContentRoot() const { return m_ContentRoot; }

  QSize sizeHint() const override { return QSize(800, 280); }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void ModelImported();

private:
  void OnItemDoubleClicked(QListWidgetItem *item);

  // Check if file is an image type
  static bool IsImageFile(const std::string &extension);

  BrowserListWidget *m_ListWidget;
  BrowserItemDelegate *m_ItemDelegate;
  std::string m_CurrentPath;
  std::string m_ContentRoot = "c:\\qcontent\\";
  std::stack<std::string> m_History;

  // Custom icons
  QIcon m_FolderIcon;
  QIcon m_FileIcon;
  QIcon m_ModelIcon;
  QIcon m_ScriptIcon;
  QIcon m_ImageIcon; // Fallback for images if thumbnail fails
};
