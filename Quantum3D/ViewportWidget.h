#pragma once

#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QtWidgets/QWidget>
#include <memory>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

// Forward declarations
namespace Vivid {
class VividDevice;
class VividRenderer;
class Draw2D;
} // namespace Vivid

namespace Quantum {
class SceneGraph;
class GraphNode;
class SceneRenderer;
class LightNode;
} // namespace Quantum

class EditorCamera; // Forward declaration

class ViewportWidget : public QWidget {
  Q_OBJECT

public:
  ViewportWidget(QWidget *parent = nullptr);
  ~ViewportWidget();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  QPaintEngine *paintEngine() const override { return nullptr; }

  // Input Events
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private slots:
  void renderFrame();

public slots:
  void OnModelImported(); // Called when BrowserWidget imports a model

private:
  void initVulkan();
  void cleanupVulkan();
  void recreateSwapChain();
  void initScene();
  void updateCamera(float deltaTime);

  Vivid::VividDevice *m_Device = nullptr;
  Vivid::VividRenderer *m_Renderer = nullptr;
  std::unique_ptr<Vivid::Draw2D> m_Draw2D;
  QTimer *m_RenderTimer = nullptr;

  // Scene Renderer
  std::unique_ptr<Quantum::SceneRenderer> m_SceneRenderer;

  // Scene
  std::shared_ptr<Quantum::SceneGraph> m_SceneGraph;
  std::shared_ptr<Quantum::GraphNode> m_TestModel;
  std::shared_ptr<Quantum::GraphNode> m_SelectedNode; // Currently selected node
  std::shared_ptr<Quantum::LightNode> m_MainLight2;   // Test light

public:
  void SetSelectedNode(std::shared_ptr<Quantum::GraphNode> node);

private:
  std::shared_ptr<Quantum::LightNode> m_MainLight; // Test light

  bool m_VulkanInitialized = false;
  bool m_SceneInitialized = false;
  bool m_NeedsResize = false;
  int m_Width = 0;
  int m_Height = 0;

  // Camera Control
  bool m_IsLooking = false;
  QPoint m_LastMousePos;
  QPoint m_OriginalCursorPos;
  std::unordered_map<int, bool> m_KeysDown;

  std::unique_ptr<EditorCamera> m_EditorCamera;
};
