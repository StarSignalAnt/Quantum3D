#include "VividApplication.h"
#include "pch.h"
#include <iostream>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include "AppUI.h"
#include <GLFW/glfw3.h>

namespace Vivid {

int VividApplication::s_FrameWidth = 0;
int VividApplication::s_FrameHeight = 0;
float VividApplication::s_DPIScale = 1.0f;

VividApplication::VividApplication(int width, int height, const char *title)
    : m_Width(width), m_Height(height), m_Title(title), m_Running(true),
      m_Window(nullptr), m_DevicePtr(nullptr), m_RendererPtr(nullptr) {
  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  m_Window = glfwCreateWindow(m_Width, m_Height, m_Title, nullptr, nullptr);
  if (!m_Window) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }

  // Detect and Set DPI Scale
  float xScale = 1.0f, yScale = 1.0f;
  glfwGetWindowContentScale(m_Window, &xScale, &yScale);
  printf("DPI Scale Detected: %.2f, %.2f\n", xScale, yScale);
  // We use the larger of the two, or just X, as they should match
  AppUI::SetScale(xScale);

  SetupInputCallbacks();
  InitVulkan();
}

VividApplication::~VividApplication() {
  Cleanup();
  if (m_Window) {
    glfwDestroyWindow(m_Window);
  }
  glfwTerminate();
}

void VividApplication::SetupInputCallbacks() {
  // Store 'this' pointer for callbacks
  glfwSetWindowUserPointer(m_Window, this);

  glfwSetCursorPosCallback(m_Window, MousePositionCallback);
  glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
  glfwSetKeyCallback(m_Window, KeyCallback);
  glfwSetKeyCallback(m_Window, KeyCallback);
  glfwSetScrollCallback(m_Window, ScrollCallback);
  glfwSetWindowSizeCallback(m_Window, WindowResizeCallback);
}

void VividApplication::MousePositionCallback(GLFWwindow *window, double xpos,
                                             double ypos) {
  auto *app = static_cast<VividApplication *>(glfwGetWindowUserPointer(window));
  if (app) {
    // printf("GLFW Move: %.2f %.2f\n", xpos, ypos); // Commented out to reduce
    // spam unless needed
    app->m_Input.SetMousePosition(static_cast<float>(xpos),
                                  static_cast<float>(ypos));
  }
}

void VividApplication::MouseButtonCallback(GLFWwindow *window, int button,
                                           int action, int mods) {
  (void)mods;
  auto *app = static_cast<VividApplication *>(glfwGetWindowUserPointer(window));
  if (app && button >= 0 &&
      button < static_cast<int>(MouseButton::MaxButtons)) {
    printf("GLFW Click: %d Action: %d\n", button, action);
    app->m_Input.SetMouseButton(static_cast<MouseButton>(button),
                                action == GLFW_PRESS);
  }
}

void VividApplication::KeyCallback(GLFWwindow *window, int key, int scancode,
                                   int action, int mods) {
  (void)scancode;
  (void)mods;
  auto *app = static_cast<VividApplication *>(glfwGetWindowUserPointer(window));
  if (app && key >= 0 && key < static_cast<int>(Key::MaxKeys)) {
    bool pressed = (action == GLFW_PRESS || action == GLFW_REPEAT);
    app->m_Input.SetKey(static_cast<Key>(key), pressed);
  }
}

void VividApplication::ScrollCallback(GLFWwindow *window, double xoffset,
                                      double yoffset) {
  auto *app = static_cast<VividApplication *>(glfwGetWindowUserPointer(window));
  if (app) {
    app->m_Input.SetMouseScroll(static_cast<float>(xoffset),
                                static_cast<float>(yoffset));
  }
}

void VividApplication::WindowResizeCallback(GLFWwindow *window, int width,
                                            int height) {
  auto *app = static_cast<VividApplication *>(glfwGetWindowUserPointer(window));
  if (app) {
    app->m_Width = width;
    app->m_Height = height;
    if (app->m_RendererPtr) {
      // app->m_RendererPtr->Resize(width, height); // Assuming renderer has
      // resize? If not, we skip for now, but UI needs it.
    }
    app->OnResize(width, height);
  }
}

void VividApplication::InitVulkan() {
  m_DevicePtr = new VividDevice(m_Window, m_Title);
  m_RendererPtr = new VividRenderer(m_DevicePtr, m_Width, m_Height);
}

void VividApplication::Cleanup() {
  delete m_RendererPtr;
  delete m_DevicePtr;
}

void VividApplication::Run() {
  Init();

  while (m_Running && !glfwWindowShouldClose(m_Window)) {
    // Update input state first (preserves last frame state)
    m_Input.Update();

    glfwPollEvents();

    Update();

    if (m_RendererPtr && m_RendererPtr->BeginFrame()) {
      Render();
      m_RendererPtr->EndFrame();
    }
  }
}

VkCommandBuffer VividApplication::GetCommandBuffer() {
  if (m_RendererPtr) {
    return m_RendererPtr->GetCommandBuffer();
  }
  return VK_NULL_HANDLE;
}
} // namespace Vivid
