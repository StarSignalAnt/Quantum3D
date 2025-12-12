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
#include "../QuantumEngine/GraphNode.h"
#include "../QuantumEngine/LightNode.h"
#include "../QuantumEngine/ModelImporter.h"
#include "../QuantumEngine/SceneGraph.h"
#include "../QuantumEngine/SceneRenderer.h"
#include "../QuantumEngine/VividDevice.h"
#include "../QuantumEngine/VividRenderer.h"

#include "../QuantumEngine/VividApplication.h"

ViewportWidget::ViewportWidget(QWidget *parent) : QWidget(parent) {
  // Set the widget to be a native window so we get a real HWND
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);

  // Enable keyboard focus
  setFocusPolicy(Qt::StrongFocus);

  // Set minimum size
  setMinimumSize(100, 100);

  // Defer Vulkan initialization until first paint/resize
  // This ensures the widget has a valid HWND
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

    // Create renderer
    m_Renderer = new Vivid::VividRenderer(m_Device, m_Width, m_Height);

    // Create scene renderer
    m_SceneRenderer =
        std::make_unique<Quantum::SceneRenderer>(m_Device, m_Renderer);
    m_SceneRenderer->Initialize();

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
    // Create scene graph
    m_SceneGraph = std::make_shared<Quantum::SceneGraph>();

    // Import test model (path relative to x64/Debug where exe runs)
    m_TestModel =
        Quantum::ModelImporter::ImportEntity("test/monkey.fbx", m_Device);

    if (m_TestModel) {
      m_SceneGraph->GetRoot()->AddChild(m_TestModel);

      // Position the model
      m_TestModel->SetLocalPosition(0.0f, 0.0f, -5.0f);

      std::cout << "Test model loaded successfully with "
                << m_TestModel->GetMeshCount() << " meshes" << std::endl;
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
    m_MainLight->SetLocalPosition(2.0f, 4.0f, 2.0f); // Position light
    m_MainLight->SetRange(60.0f);
    m_SceneGraph->AddLight(m_MainLight);

    // Initialize Editor Camera
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

  if (m_RenderTimer) {
    m_RenderTimer->stop();
    delete m_RenderTimer;
    m_RenderTimer = nullptr;
  }

  // Clear scene
  m_TestModel.reset();
  m_SceneGraph.reset();

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

    delete m_Renderer;
    m_Renderer = new Vivid::VividRenderer(m_Device, m_Width, m_Height);
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

  // Render frame
  if (m_Renderer && m_Renderer->BeginFrame()) {
    // Use scene renderer to render the scene
    if (m_SceneRenderer) {
      m_SceneRenderer->RenderScene(m_Renderer->GetCommandBuffer(), m_Width,
                                   m_Height);
    }
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
  if (event->button() == Qt::RightButton) {
    m_IsLooking = false;
    // Restore cursor to original position
    QCursor::setPos(m_OriginalCursorPos);
    setCursor(Qt::ArrowCursor);
  }
  QWidget::mouseReleaseEvent(event);
}

void ViewportWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_IsLooking) {
    // We are in infinite look mode
    // Calculate delta from the center of the widget (where we reset the cursor)
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
