#pragma once

#include "Font.h"
#include "Texture2D.h"
#include "glm/glm.hpp"
#include <string>

namespace Vivid {

class VividDevice;

class UITheme {
public:
  UITheme();
  virtual ~UITheme();

  // Initialize theme - loads textures
  virtual void Init(VividDevice *device) = 0;

  // Colors
  void SetBackgroundColor(const glm::vec4 &color) { m_BackgroundColor = color; }
  void SetForegroundColor(const glm::vec4 &color) { m_ForegroundColor = color; }

  glm::vec4 GetBackgroundColor() const { return m_BackgroundColor; }
  glm::vec4 GetForegroundColor() const { return m_ForegroundColor; }

  // Window Colors
  void SetWindowBackgroundColor(const glm::vec4 &color) {
    m_WindowBackgroundColor = color;
  }
  void SetTitleBarColor(const glm::vec4 &color) { m_TitleBarColor = color; }
  void SetTitleTextColor(const glm::vec4 &color) { m_TitleTextColor = color; }
  void SetBorderColor(const glm::vec4 &color) { m_BorderColor = color; }
  void SetTabActiveColor(const glm::vec4 &color) { m_TabActiveColor = color; }
  void SetTabInactiveColor(const glm::vec4 &color) {
    m_TabInactiveColor = color;
  }

  glm::vec4 GetWindowBackgroundColor() const { return m_WindowBackgroundColor; }
  glm::vec4 GetTitleBarColor() const { return m_TitleBarColor; }
  glm::vec4 GetTitleTextColor() const { return m_TitleTextColor; }
  glm::vec4 GetBorderColor() const { return m_BorderColor; }
  glm::vec4 GetTabActiveColor() const { return m_TabActiveColor; }
  glm::vec4 GetTabInactiveColor() const { return m_TabInactiveColor; }

  // Frame texture (for buttons, panels, etc.)
  Texture2D *GetFrameTexture() const { return m_FrameTexture; }
  Texture2D *GetHeaderTexture() const { return m_HeaderTexture; }
  Texture2D *GetWhiteTexture() const { return m_WhiteTexture; }

  // Font for UI text
  Font *GetFont() const { return m_Font; }

  // Theme name
  virtual const char *GetName() const = 0;

protected:
  glm::vec4 m_BackgroundColor;
  glm::vec4 m_ForegroundColor;

  // Window styling
  glm::vec4 m_WindowBackgroundColor;
  glm::vec4 m_TitleBarColor;
  glm::vec4 m_TitleTextColor;
  glm::vec4 m_BorderColor;
  glm::vec4 m_TabActiveColor;
  glm::vec4 m_TabInactiveColor;
  Texture2D *m_FrameTexture;
  Texture2D *m_HeaderTexture;
  Texture2D *m_WhiteTexture;
  Font *m_Font;
  VividDevice *m_DevicePtr;
};

} // namespace Vivid
