#pragma once

#include "AppInput.h"
#include "VividDevice.h"
#include "VividRenderer.h"
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Vivid {
class VividApplication {
public:
  VividApplication(int width, int height, const char *title);
  virtual ~VividApplication();

  virtual void Init() {}
  virtual void Update() {}
  virtual void Render() {}
  virtual void OnResize(int width, int height) {}

  void Run();

  // Global Frame Size
  static void SetFrameWidth(int width) { s_FrameWidth = width; }
  static void SetFrameHeight(int height) { s_FrameHeight = height; }
  static int GetFrameWidth() { return s_FrameWidth; }
  static int GetFrameHeight() { return s_FrameHeight; }

  // Global DPI Scale
  static void SetDPIScale(float scale) { s_DPIScale = scale; }
  static float GetDPIScale() { return s_DPIScale; }

  // Input access
  AppInput &GetInput() { return m_Input; }
  const AppInput &GetInput() const { return m_Input; }

protected:
  void InitVulkan();
  void MainLoop();
  void Cleanup();
  void SetupInputCallbacks();

  VkCommandBuffer GetCommandBuffer();

  // GLFW callbacks (static for C callback compatibility)
  static void MousePositionCallback(GLFWwindow *window, double xpos,
                                    double ypos);
  static void MouseButtonCallback(GLFWwindow *window, int button, int action,
                                  int mods);
  static void KeyCallback(GLFWwindow *window, int key, int scancode, int action,
                          int mods);
  static void ScrollCallback(GLFWwindow *window, double xoffset,
                             double yoffset);
  static void WindowResizeCallback(GLFWwindow *window, int width, int height);

  GLFWwindow *m_Window;
  int m_Width;
  int m_Height;
  const char *m_Title;
  bool m_Running;

  VividDevice *m_DevicePtr;
  VividRenderer *m_RendererPtr;
  AppInput m_Input;

  static int s_FrameWidth;
  static int s_FrameHeight;
  static float s_DPIScale;
};
} // namespace Vivid
