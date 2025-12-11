#pragma once

#include "UIControl.h"
#include <functional>

namespace Vivid {

class IHorizontalScroller : public UIControl {
public:
  IHorizontalScroller();
  virtual ~IHorizontalScroller() = default;

  // Setters
  void SetContentSize(float size) { m_ContentSize = size; }
  void SetViewSize(float size) { m_ViewSize = size; }
  void SetValue(float value); // 0.0 to 1.0

  // Getters
  float GetValue() const { return m_Value; }

  // Scroll Callback
  void SetOnScrollCallback(std::function<void(float)> callback) {
    m_OnScrollCallback = callback;
  }

protected:
  void OnDraw(Draw2D *draw2D) override;
  void OnMouseDown(MouseButton button) override;
  void OnMouseUp(MouseButton button) override;
  void OnMouseMove(const glm::vec2 &position) override;

private:
  float m_ContentSize;
  float m_ViewSize;
  float m_Value; // 0.0f (Left) to 1.0f (Right)

  bool m_IsDragging;
  float m_DragStartMouseX;
  float m_DragStartValue;
  float m_LastMouseX; // Cache for OnMouseDown

  std::function<void(float)> m_OnScrollCallback;

  // Internal helper to calculate thumb rect
  // Returns x, width (relative to control position)
  void GetThumbMetrics(float &outX, float &outWidth) const;
};

} // namespace Vivid
