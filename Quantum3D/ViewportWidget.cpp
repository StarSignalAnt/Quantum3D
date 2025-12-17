#include "ViewportWidget.h"
#include "EditorCamera.h"
#include "stdafx.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

// Include QuantumEngine headers
#include "../QuantumEngine/CameraNode.h"
#include "../QuantumEngine/Draw2D.h" // Added for debug overlay
#include "../QuantumEngine/GraphNode.h"
#include "../QuantumEngine/LightNode.h"
#include "../QuantumEngine/ModelImporter.h"
#include "../QuantumEngine/RenderingPipelines.h"
#include "../QuantumEngine/SceneGraph.h"
#include "../QuantumEngine/SceneRenderer.h"
#include "../QuantumEngine/VividDevice.h"
#include "../QuantumEngine/VividRenderer.h"

#include "../QuantumEngine/VividApplication.h"
#include "EngineGlobals.h"

ViewportWidget::ViewportWidget(QWidget *parent) : QWidget(parent) {
  // Set the widget to be a native window so we get a real HWND
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);

  // Enable keyboard focus
  setFocusPolicy(Qt::StrongFocus);

  // Set minimum size
  setMinimumSize(100, 100);

  if (!EngineGlobals::EditorScene) {
    EngineGlobals::EditorScene = std::make_shared<Quantum::SceneGraph>();
  }
  m_SceneGraph = EngineGlobals::EditorScene;
}

ViewportWidget::~ViewportWidget() { cleanupVulkan(); }

void ViewportWidget::initVulkan() {
  if (m_VulkanInitialized) {
    return;
  }

  try {
    // Get the native window handle
    HWND hwnd = reinterpret_cast<HWND>(winId());
    HINSTANCE hinstance = GetModuleHandle(nullptr);

    if (!hwnd) {
      std::cerr << "Failed to get native window handle!" << std::endl;
      return;
    }

    // High DPI Support: Calculate physical pixels
    m_Width = width() * devicePixelRatio();
    m_Height = height() * devicePixelRatio();

    // Set Global Application State
    Vivid::VividApplication::SetDPIScale(devicePixelRatio());
    Vivid::VividApplication::SetFrameWidth(m_Width);
    Vivid::VividApplication::SetFrameHeight(m_Height);

    // Create Vulkan device using Win32 surface
    m_Device = new Vivid::VividDevice(hwnd, hinstance, "Quantum3D Viewport");
    EngineGlobals::VulkanDevice = m_Device; // Expose device globally

    // Create renderer
    m_Renderer = new Vivid::VividRenderer(m_Device, m_Width, m_Height);

    // Create scene renderer
    m_SceneRenderer =
        std::make_unique<Quantum::SceneRenderer>(m_Device, m_Renderer);
    m_SceneRenderer->Initialize();

    // Create 2D renderer for debug overlay
    m_Draw2D =
        std::make_unique<Vivid::Draw2D>(m_Device, m_Renderer->GetRenderPass());

    // Setup render timer for continuous rendering (60 FPS target)
    m_RenderTimer = new QTimer(this);
    connect(m_RenderTimer, &QTimer::timeout, this,
            &ViewportWidget::renderFrame);
    m_RenderTimer->start(16); // ~60 FPS

    m_VulkanInitialized = true;
    std::cout << "Vulkan initialized successfully for ViewportWidget"
              << std::endl;

    // Initialize scene
    initScene();

  } catch (const std::exception &e) {
    std::cerr << "Failed to initialize Vulkan: " << e.what() << std::endl;
  }
}

void ViewportWidget::initScene() {
  if (m_SceneInitialized)
    return;

  // Debug Vertex Layout
  qDebug() << "Vertex3D Size:" << sizeof(Quantum::Vertex3D);
  qDebug() << "Offset Position:" << offsetof(Quantum::Vertex3D, position);
  qDebug() << "Offset Normal:" << offsetof(Quantum::Vertex3D, normal);
  qDebug() << "Offset UV:" << offsetof(Quantum::Vertex3D, uv);
  qDebug() << "Offset Tangent:" << offsetof(Quantum::Vertex3D, tangent);
  qDebug() << "Offset Bitangent:" << offsetof(Quantum::Vertex3D, bitangent);

  try {
    // EngineGlobals::EditorScene is already ensured in Constructor/initVulkan
    // sequence if needed, but good to check.
    if (!EngineGlobals::EditorScene) {
      EngineGlobals::EditorScene = std::make_shared<Quantum::SceneGraph>();
    }
    m_SceneGraph = EngineGlobals::EditorScene;

    if (m_TestModel) {
      // m_SceneGraph->GetRoot()->AddChild(m_TestModel);
    } else {
      std::cerr << "Failed to load test model!" << std::endl;
    }

    // Create and setup camera
    auto camera = std::make_shared<Quantum::CameraNode>("MainCamera");

    // Position camera at (0, 5, 5), looking at origin
    camera->LookAt(glm::vec3(0.0f, 5.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                   glm::vec3(0.0f, 1.0f, 0.0f));

    m_SceneGraph->GetRoot()->AddChild(camera);
    m_SceneGraph->SetCurrentCamera(camera);

    // Create Test Light
    m_MainLight = std::make_shared<Quantum::LightNode>("MainLight");
    m_MainLight->SetColor(glm::vec3(150.0f, 150.0f, 150.0f));
    m_MainLight->SetLocalPosition(0.0f, 2.0f,
                                  5.0f); // Position light in FRONT ensures
                                         // intuitive face mapping (Face 5)
    m_MainLight->SetRange(30.0f);

    auto l2 = std::make_shared<Quantum::LightNode>("MainLight2");
    l2->SetColor(glm::vec3(0, 150.0f, 150.0f));
    l2->SetLocalPosition(0.0f, 15.0f,
                         15.0f); // Position light in FRONT ensures
    // intuitive face mapping (Face 5)
    l2->SetRange(100.0f);
    m_MainLight2 = l2;
    m_SceneGraph->AddLight(l2);

    m_SceneGraph->AddLight(m_MainLight);

    // Scaling the monkey down because import scalefactor is 100
    // We can do this on the root node or the monkey node if we had a reference
    // But since we import via file drop or code, let's look for it?
    // For now, relying on the user to drop it?
    // Wait, m_TestModel is loaded in initScene.
    if (m_TestModel) {
      m_TestModel->SetLocalScale(0.01f);
      m_SceneGraph->GetRoot()->AddChild(m_TestModel); // Re-enable adding child
      m_SelectedNode = m_TestModel;
    }
    m_EditorCamera = std::make_unique<EditorCamera>();
    m_EditorCamera->SetCamera(camera);

    // Set scene graph on the renderer
    m_SceneRenderer->SetSceneGraph(m_SceneGraph);

    m_SceneInitialized = true;
    std::cout << "Scene initialized successfully" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Failed to initialize scene: " << e.what() << std::endl;
  }
}

void ViewportWidget::cleanupVulkan() {
  if (m_Device) {
    vkDeviceWaitIdle(m_Device->GetDevice());
  }

  // Cleanup Draw2D
  m_Draw2D.reset();

  if (m_RenderTimer) {
    m_RenderTimer->stop();
    delete m_RenderTimer;
    m_RenderTimer = nullptr;
  }

  // Clear scene
  m_TestModel.reset();
  m_SceneGraph.reset();

  // CRITICAL: Clear the global scene reference as it holds GPU resources
  // tied to the device we are about to destroy.
  EngineGlobals::EditorScene.reset();

  // Shutdown the pipeline singleton BEFORE destroying the renderer
  // Otherwise it holds stale pointers to the destroyed VividRenderer
  Quantum::RenderingPipelines::Get().Shutdown();

  // Cleanup scene renderer
  m_SceneRenderer.reset();

  if (m_Renderer) {
    delete m_Renderer;
    m_Renderer = nullptr;
  }

  if (m_Device) {
    delete m_Device;
    m_Device = nullptr;
  }

  m_VulkanInitialized = false;
  m_SceneInitialized = false;
}

void ViewportWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  // High DPI Support: Use physical pixels
  m_Width = event->size().width() * devicePixelRatio();
  m_Height = event->size().height() * devicePixelRatio();

  // Set Global DPI Scale
  Vivid::VividApplication::SetDPIScale(devicePixelRatio());

  if (!m_VulkanInitialized) {
    // Initialize Vulkan on first resize when we have valid dimensions
    if (m_Width > 0 && m_Height > 0) {
      initVulkan();
    }
  } else {
    // Mark that we need to recreate swap chain
    m_NeedsResize = true;
  }
}

void ViewportWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  // Don't use Qt painting - Vulkan handles rendering
  if (!m_VulkanInitialized && width() > 0 && height() > 0) {
    initVulkan();
  }
}

void ViewportWidget::recreateSwapChain() {
  if (!m_Device || m_Width <= 0 || m_Height <= 0) {
    return;
  }

  // Recreate the renderer with new dimensions
  try {
    vkDeviceWaitIdle(m_Device->GetDevice());

    // Set Global Application State before recreation
    Vivid::VividApplication::SetFrameWidth(m_Width);
    Vivid::VividApplication::SetFrameHeight(m_Height);

    // Invalidate pipeline objects BEFORE destroying the old renderer
    // (pipelines reference the old render pass)
    // This keeps registrations so pipelines can be recreated lazily
    Quantum::RenderingPipelines::Get().InvalidatePipelines();

    delete m_Renderer;
    m_Renderer = new Vivid::VividRenderer(m_Device, m_Width, m_Height);

    // Update RenderingPipelines with the NEW render pass
    if (m_SceneRenderer) {
      Quantum::RenderingPipelines::Get().Initialize(
          m_Device, m_Renderer->GetRenderPass(),
          {m_SceneRenderer->GetGlobalSetLayout(),
           m_SceneRenderer->GetDescriptorSetLayout()});
    }

    m_NeedsResize = false;
  } catch (const std::exception &e) {
    std::cerr << "Failed to recreate swap chain: " << e.what() << std::endl;
  }
}

void ViewportWidget::renderFrame() {
  if (!m_VulkanInitialized || !m_Renderer) {
    return;
  }

  // Handle resize if needed
  if (m_NeedsResize) {
    recreateSwapChain();
  }

  // Robustness check: Ensure swapchain matches physical widget size
  if (m_Renderer) {
    int pixelWidth = width() * devicePixelRatio();
    int pixelHeight = height() * devicePixelRatio();

    if (pixelWidth != m_Width || pixelHeight != m_Height) {
      m_Width = pixelWidth;
      m_Height = pixelHeight;
      m_NeedsResize = true;
      recreateSwapChain();
    }
  }

  // Update camera logic
  updateCamera(0.016f); // Approx 60 FPS

  // Render frame using split-phase for shadow pass injection
  if (m_Renderer && m_Renderer->BeginFrameCommandBuffer()) {
    // Phase 1: Shadow pass (before main render pass)
    if (m_SceneRenderer) {
      m_SceneRenderer->RenderShadowPass(m_Renderer->GetCommandBuffer());
    }

    // Phase 2: Begin main render pass
    m_Renderer->BeginMainRenderPass();

    // Phase 3: Main scene rendering
    if (m_SceneRenderer) {
      // Update gizmo view state for hit detection
      if (m_EditorCamera) {
        glm::mat4 view = m_EditorCamera->GetViewMatrix();
        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f), static_cast<float>(m_Width) / m_Height, 0.01f,
            1000.0f);
        m_SceneRenderer->SetGizmoViewState(view, proj, m_Width, m_Height);
      }

      m_SceneRenderer->RenderScene(m_Renderer->GetCommandBuffer(), m_Width,
                                   m_Height);

      // Render Selection
      if (m_SelectedNode) {
        //     m_SceneRenderer->RenderSelection(m_Renderer->GetCommandBuffer(),
        //                                    m_SelectedNode);
      }

      // Phase 3.5: Debug overlay (Draw2D)
      if (m_Draw2D) {
        // m_Draw2D->Begin(m_Renderer);
        // m_SceneRenderer->RenderShadowDebug(m_Draw2D.get());
        // m_Draw2D->End();
      }
    }

    // Phase 4: End frame
    m_Renderer->EndFrame();
  }
}

// =================================================================================================
// Camera Control
// =================================================================================================

void ViewportWidget::updateCamera(float deltaTime) {
  if (!m_EditorCamera)
    return;

  // Handle Movement (Keyboard)
  glm::vec3 inputDir(0.0f);

  // W/S - Forward/Backward (Z)
  if (m_KeysDown[Qt::Key_W])
    inputDir.z += 1.0f;
  if (m_KeysDown[Qt::Key_S])
    inputDir.z -= 1.0f;

  // A/D - Left/Right (X)
  if (m_KeysDown[Qt::Key_D])
    inputDir.x += 1.0f;
  if (m_KeysDown[Qt::Key_A])
    inputDir.x -= 1.0f;

  // Q/E - Down/Up (Y)
  if (m_KeysDown[Qt::Key_E])
    inputDir.y += 1.0f;
  if (m_KeysDown[Qt::Key_Q])
    inputDir.y -= 1.0f;

  m_EditorCamera->Move(inputDir, deltaTime);

  // Space - Move Light to Camera
  if (m_KeysDown[Qt::Key_Space]) {
    if (m_MainLight && m_SceneGraph && m_SceneGraph->GetCurrentCamera()) {
      m_MainLight->SetLocalPosition(
          m_SceneGraph->GetCurrentCamera()->GetLocalPosition());
    }
  }
  if (m_KeysDown[Qt::Key_T]) {
    if (m_MainLight2 && m_SceneGraph && m_SceneGraph->GetCurrentCamera()) {
      m_MainLight2->SetLocalPosition(
          m_SceneGraph->GetCurrentCamera()->GetLocalPosition());
    }
  }
}

void ViewportWidget::keyPressEvent(QKeyEvent *event) {
  m_KeysDown[event->key()] = true;
  QWidget::keyPressEvent(event);
}

void ViewportWidget::keyReleaseEvent(QKeyEvent *event) {
  m_KeysDown[event->key()] = false;
  QWidget::keyReleaseEvent(event);
}

void ViewportWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int mouseX = static_cast<int>(event->position().x());
    int mouseY = static_cast<int>(event->position().y());

    // First, check if gizmo wants to handle this click
    if (m_SceneRenderer &&
        m_SceneRenderer->OnGizmoMouseClicked(mouseX, mouseY, true,width(),height())) {
      // Gizmo consumed the click, don't do node selection
    } else if (m_SceneGraph) {
      // Gizmo didn't consume, do normal node selection
      std::shared_ptr<Quantum::GraphNode> selected = m_SceneGraph->SelectEntity(
          event->position().x(), event->position().y(), width(), height());

      SetSelectedNode(selected);
      if (selected) {
        std::cout << "Selected Node: " << selected->GetName() << std::endl;
      } else {
        std::cout << "Selection Cleared" << std::endl;
      }
    }
  }

  if (event->button() == Qt::RightButton) {
    m_IsLooking = true;
    m_OriginalCursorPos = QCursor::pos(); // Save global position
    setCursor(Qt::BlankCursor);

    // Center cursor to start avoiding edges immediately
    QCursor::setPos(mapToGlobal(rect().center()));
    m_LastMousePos = mapToGlobal(rect().center());
  }
  QWidget::mousePressEvent(event);
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    // Forward mouse release to gizmo
    if (m_SceneRenderer) {
      m_SceneRenderer->OnGizmoMouseClicked(
          static_cast<int>(event->position().x()),
          static_cast<int>(event->position().y()), false,width(),height());
    }
  }
  if (event->button() == Qt::RightButton) {
    m_IsLooking = false;
    // Restore cursor to original position
    QCursor::setPos(m_OriginalCursorPos);
    setCursor(Qt::ArrowCursor);
  }
  QWidget::mouseReleaseEvent(event);
}

void ViewportWidget::mouseMoveEvent(QMouseEvent *event) {
  // Forward mouse move to gizmo (for dragging)
  if (m_SceneRenderer) {
    m_SceneRenderer->OnGizmoMouseMoved(static_cast<int>(event->position().x()),
                                       static_cast<int>(event->position().y()));
  }

  if (m_IsLooking) {
    // We are in infinite look mode
    // Calculate delta from the center of the widget (where we reset the
    // cursor)
    QPoint centerPos = mapToGlobal(rect().center());
    QPoint currentPos = event->globalPosition().toPoint();
    QPoint delta = currentPos - centerPos;

    // Ignore the move event if it's the result of our recentering
    if (delta.isNull()) {
      return;
    }

    // Update yaw/pitch
    if (m_EditorCamera) {
      m_EditorCamera->Rotate(delta.x(), delta.y());
    }

    // Recenter cursor to allow infinite movement
    QCursor::setPos(centerPos);
    m_LastMousePos = centerPos;
  }
  QWidget::mouseMoveEvent(event);
}

void ViewportWidget::OnModelImported() {
  if (m_SceneRenderer) {
    std::cout
        << "[ViewportWidget] OnModelImported: Refreshing material textures..."
        << std::endl;
    m_SceneRenderer->RefreshMaterialTextures();
  }
}

void ViewportWidget::SetSelectedNode(std::shared_ptr<Quantum::GraphNode> node) {
  m_SelectedNode = node;
  // Update gizmo position and target node
  if (m_SceneRenderer) {
    if (node) {
      m_SceneRenderer->SetGizmoPosition(node->GetWorldPosition());
      m_SceneRenderer->SetGizmoTargetNode(node);
    } else {
      m_SceneRenderer->SetGizmoTargetNode(nullptr);
    }
  }
}
