#include "IVerticalScroller.h"
#include "AppUI.h"
#include "Draw2D.h"
#include "UITheme.h"
#include <algorithm>
#include <cmath>

namespace Vivid {

IVerticalScroller::IVerticalScroller()
    : m_ContentSize(100.0f), m_ViewSize(100.0f), m_Value(0.0f),
      m_IsDragging(false), m_DragStartMouseY(0.0f), m_DragStartValue(0.0f),
      m_LastMouseY(0.0f) {
  SetSize(glm::vec2(12.0f, 100.0f)); // Default width
}

void IVerticalScroller::SetValue(float value) {
  m_Value = std::clamp(value, 0.0f, 1.0f);
  if (m_OnScrollCallback) {
    m_OnScrollCallback(m_Value);
  }
}

void IVerticalScroller::GetThumbMetrics(float &outY, float &outHeight) const {
  float scale = AppUI::GetScale();
  float totalHeight = GetSize().y / scale; // Convert pixels to logical

  // Calculate proportion
  float ratio = 1.0f;
  if (m_ContentSize > 0.0f) {
    ratio = m_ViewSize / m_ContentSize;
  }
  ratio = std::clamp(ratio, 0.1f, 1.0f); // Min thumb size 10%

  float thumbH = totalHeight * ratio;
  // Enforce min logical height for thumb (e.g. 20 logical pixels)
  float minThumbH = 20.0f;
  if (thumbH < minThumbH)
    thumbH = minThumbH;

  if (thumbH > totalHeight)
    thumbH = totalHeight;

  outHeight = thumbH;

  // Available track for movement (logical)
  float trackSpace = totalHeight - thumbH;
  outY = trackSpace * m_Value;
}

void IVerticalScroller::OnDraw(Draw2D *draw2D) {
  if (!draw2D)
    return;

  UITheme *theme = GetTheme();
  if (!theme)
    return;

  Texture2D *whiteTex = theme->GetWhiteTexture();
  if (!whiteTex)
    return;

  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 size = GetSize(); // Scaled by AppUI in UIControl::GetSize

  // Background (Track)
  draw2D->DrawTexture(absPos, size, whiteTex,
                      glm::vec4(0.15f, 0.15f, 0.15f, 1.0f), BlendMode::Solid);

  // Thumb
  if (m_ContentSize > m_ViewSize) {
    float thumbY, thumbH;
    GetThumbMetrics(thumbY, thumbH);

    // Scale metrics to pixels for drawing
    float scale = AppUI::GetScale();
    float pixelY = thumbY * scale;
    float pixelH = thumbH * scale;

    glm::vec2 thumbPos =
        glm::vec2(absPos.x + (2.0f * scale), absPos.y + pixelY);
    glm::vec2 thumbSize = glm::vec2(size.x - (4.0f * scale), pixelH);

    // Thumb color: lighter grey when dragging
    glm::vec4 thumbColor = m_IsDragging ? glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)
                                        : glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);

    draw2D->DrawTexture(thumbPos, thumbSize, whiteTex, thumbColor,
                        BlendMode::Solid);
  }
}

void IVerticalScroller::OnMouseDown(MouseButton button) {
  if (button == MouseButton::Left) {
    if (m_ContentSize <= m_ViewSize)
      return;

    glm::vec2 absPos = GetAbsolutePosition();
    float scale = AppUI::GetScale();
    float localMouseY = (m_LastMouseY - absPos.y) / scale; // Convert to logical

    float thumbY, thumbH;
    GetThumbMetrics(thumbY, thumbH);

    // Check if clicked ON thumb
    if (localMouseY >= thumbY && localMouseY <= thumbY + thumbH) {
      m_IsDragging = true;
      m_DragStartMouseY = m_LastMouseY; // Global pixels
      m_DragStartValue = m_Value;
    }
    // Page Jump logic could go here (if clicked track)
  }
}

void IVerticalScroller::OnMouseUp(MouseButton button) {
  if (button == MouseButton::Left) {
    m_IsDragging = false;
  }
}

void IVerticalScroller::OnMouseMove(const glm::vec2 &position) {
  // position is local
  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 globalMouse = position + absPos;
  m_LastMouseY = globalMouse.y;

  if (m_IsDragging) {
    float currentMouseY = globalMouse.y;
    float deltaY = currentMouseY - m_DragStartMouseY; // Pixels

    float scale = AppUI::GetScale();
    float totalHeight = GetSize().y / scale; // Logical height

    float thumbY, thumbH;
    GetThumbMetrics(thumbY, thumbH);

    float trackSpace = totalHeight - thumbH;
    if (trackSpace > 0.001f) {
      float valueDelta = (deltaY / scale) / trackSpace;
      SetValue(m_DragStartValue + valueDelta);
    }
  }
}

} // namespace Vivid
