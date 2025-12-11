#include "IHorizontalScroller.h"
#include "AppUI.h"
#include "Draw2D.h"
#include "UITheme.h"
#include <algorithm>
#include <cmath>

namespace Vivid {

IHorizontalScroller::IHorizontalScroller()
    : m_ContentSize(100.0f), m_ViewSize(100.0f), m_Value(0.0f),
      m_IsDragging(false), m_DragStartMouseX(0.0f), m_DragStartValue(0.0f),
      m_LastMouseX(0.0f) {
  SetSize(glm::vec2(100.0f, 12.0f)); // Default height
}

void IHorizontalScroller::SetValue(float value) {
  m_Value = std::clamp(value, 0.0f, 1.0f);
  if (m_OnScrollCallback) {
    m_OnScrollCallback(m_Value);
  }
}

void IHorizontalScroller::GetThumbMetrics(float &outX, float &outWidth) const {
  float scale = AppUI::GetScale();
  float totalWidth = GetSize().x / scale; // Convert pixels to logical

  // Calculate proportion
  float ratio = 1.0f;
  if (m_ContentSize > 0.0f) {
    ratio = m_ViewSize / m_ContentSize;
  }
  ratio = std::clamp(ratio, 0.1f, 1.0f); // Min thumb size 10%

  float thumbW = totalWidth * ratio;
  // Enforce min logical width for thumb (e.g. 20 logical pixels)
  float minThumbW = 20.0f;
  if (thumbW < minThumbW)
    thumbW = minThumbW;

  if (thumbW > totalWidth)
    thumbW = totalWidth;

  outWidth = thumbW;

  // Available track for movement (logical)
  float trackSpace = totalWidth - thumbW;
  outX = trackSpace * m_Value;
}

void IHorizontalScroller::OnDraw(Draw2D *draw2D) {
  if (!draw2D)
    return;

  UITheme *theme = GetTheme();
  if (!theme)
    return;

  Texture2D *whiteTex = theme->GetWhiteTexture();
  if (!whiteTex)
    return;

  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 size = GetSize(); // Scaled by AppUI

  // Background (Track)
  draw2D->DrawTexture(absPos, size, whiteTex,
                      glm::vec4(0.15f, 0.15f, 0.15f, 1.0f), BlendMode::Solid);

  // Thumb
  if (m_ContentSize > m_ViewSize) {
    float thumbX, thumbW;
    GetThumbMetrics(thumbX, thumbW);

    // Scale metrics to pixels
    float scale = AppUI::GetScale();
    float pixelX = thumbX * scale;
    float pixelW = thumbW * scale;

    glm::vec2 thumbPos =
        glm::vec2(absPos.x + pixelX, absPos.y + (2.0f * scale));
    glm::vec2 thumbSize = glm::vec2(pixelW, size.y - (4.0f * scale));

    // Thumb color
    glm::vec4 thumbColor = m_IsDragging ? glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)
                                        : glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);

    draw2D->DrawTexture(thumbPos, thumbSize, whiteTex, thumbColor,
                        BlendMode::Solid);
  }
}

void IHorizontalScroller::OnMouseDown(MouseButton button) {
  if (button == MouseButton::Left) {
    if (m_ContentSize <= m_ViewSize)
      return;

    glm::vec2 absPos = GetAbsolutePosition();
    float scale = AppUI::GetScale();
    float localMouseX = (m_LastMouseX - absPos.x) / scale; // Logical

    float thumbX, thumbW;
    GetThumbMetrics(thumbX, thumbW);

    // Check if clicked ON thumb
    if (localMouseX >= thumbX && localMouseX <= thumbX + thumbW) {
      m_IsDragging = true;
      m_DragStartMouseX = m_LastMouseX; // Global pixels
      m_DragStartValue = m_Value;
    }
  }
}

void IHorizontalScroller::OnMouseUp(MouseButton button) {
  if (button == MouseButton::Left) {
    m_IsDragging = false;
  }
}

void IHorizontalScroller::OnMouseMove(const glm::vec2 &position) {
  // position is local
  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 globalMouse = position + absPos;
  m_LastMouseX = globalMouse.x;

  if (m_IsDragging) {
    float currentMouseX = globalMouse.x;
    float deltaX = currentMouseX - m_DragStartMouseX; // Pixels

    float scale = AppUI::GetScale();
    float totalWidth = GetSize().x / scale; // Logical

    float thumbX, thumbW;
    GetThumbMetrics(thumbX, thumbW);

    float trackSpace = totalWidth - thumbW;
    if (trackSpace > 0.001f) {
      float valueDelta = (deltaX / scale) / trackSpace;
      SetValue(m_DragStartValue + valueDelta);
    }
  }
}

} // namespace Vivid
