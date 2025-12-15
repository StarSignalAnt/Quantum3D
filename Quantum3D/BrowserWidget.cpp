#include "BrowserWidget.h"
#include "stdafx.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QFileIconProvider>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QVBoxLayout>
#include <filesystem>

#include "../QuantumEngine/ModelImporter.h"
#include "../QuantumEngine/SceneGraph.h"
#include "EngineGlobals.h"

BrowserWidget::BrowserWidget(QWidget *parent) : QWidget(parent) {
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_ListWidget = new QListWidget(this);
  m_ListWidget->setStyleSheet("padding: 8px;");
  m_ListWidget->setViewMode(QListWidget::IconMode);
  m_ListWidget->setResizeMode(QListWidget::Adjust);
  m_ListWidget->setGridSize(QSize(64, 76));
  m_ListWidget->setIconSize(QSize(48, 48));
  m_ListWidget->setSpacing(5);
  m_ListWidget->setWordWrap(true);
  // Hide standard list view features to make it look more like a specialized
  // grid
  m_ListWidget->setUniformItemSizes(true);

  layout->addWidget(m_ListWidget);

  m_ListWidget->installEventFilter(this);
  m_ListWidget->viewport()->installEventFilter(this);
  connect(m_ListWidget, &QListWidget::itemDoubleClicked, this,
          &BrowserWidget::OnItemDoubleClicked);

  BrowsePath("c:\\qcontent\\");
}

BrowserWidget::~BrowserWidget() {}

void BrowserWidget::BrowsePath(const std::string &path) {
  m_CurrentPath = path;
  m_ListWidget->clear();

  if (!std::filesystem::exists(path)) {
    std::filesystem::create_directories(path);
  }

  QFileIconProvider iconProvider;
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

  auto addItem = [&](const std::filesystem::directory_entry &entry) {
    QFileInfo fileInfo(QString::fromStdString(entry.path().string()));
    QIcon icon = iconProvider.icon(fileInfo);

    QListWidgetItem *item = new QListWidgetItem(icon, fileInfo.fileName());
    item->setSizeHint(QSize(64, 76));
    item->setTextAlignment(Qt::AlignHCenter);
    item->setData(Qt::UserRole, QString::fromStdString(
                                    entry.path().string())); // Store full path
    m_ListWidget->addItem(item);
  };

  for (const auto &folder : folders) {
    addItem(folder);
  }

  for (const auto &file : files) {
    addItem(file);
  }
}

// ... (Wait, I should just remove them and add them at top)

// ...

void BrowserWidget::OnItemDoubleClicked(QListWidgetItem *item) {
  std::string path = item->data(Qt::UserRole).toString().toStdString();
  if (std::filesystem::is_directory(path)) {
    m_History.push(m_CurrentPath);
    BrowsePath(path);
  } else {
    // Check for supported model formats
    std::string extension = std::filesystem::path(path).extension().string();
    // Convert to lower case
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
          emit ModelImported(); // Notify ViewportWidget to refresh textures
        }
      }
    }
  }
}

bool BrowserWidget::eventFilter(QObject *watched, QEvent *event) {
  if ((watched == m_ListWidget || watched == m_ListWidget->viewport()) &&
      event->type() == QEvent::MouseButtonPress) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::XButton1) {
      if (!m_History.empty()) {
        std::string previousPath = m_History.top();
        m_History.pop();
        BrowsePath(previousPath);
        return true; // Event handled
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}
