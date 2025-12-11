#pragma once

#include "DockTypes.h"
#include "UIControl.h"
#include <functional>

namespace Vivid {

// ISplitter: A control that splits its parent area into two resizable panes
// Used for docking layouts to create resizable regions
class ISplitter : public UIControl {
public:
  ISplitter(SplitOrientation orientation = SplitOrientation::Horizontal);
  virtual ~ISplitter() = default;

  // Configuration
  void SetOrientation(SplitOrientation orientation) {
    m_Orientation = orientation;
  }
  SplitOrientation GetOrientation() const { return m_Orientation; }

  // Split ratio (0.0 to 1.0, representing position of divider)
  void SetSplitRatio(float ratio);
  float GetSplitRatio() const { return m_SplitRatio; }

  // Pane access - content should be added to these
  UIControlPtr GetPane1() const { return m_Pane1; }
  UIControlPtr GetPane2() const { return m_Pane2; }

  // Divider bar size (logical pixels)
  void SetDividerSize(float size) { m_DividerSize = size; }
  float GetDividerSize() const { return m_DividerSize; }

  // Callback when split ratio changes
  void SetOnSplitChanged(std::function<void(float)> callback) {
    m_OnSplitChanged = callback;
  }

  // Override Update to layout panes
  void Update(float deltaTime) override;

  // Override GetMinSize to report sum of child panes
  glm::vec2 GetMinSize() const override;

protected:
  void OnDraw(Draw2D *draw2D) override;
  void OnMouseDown(MouseButton button) override;
  void OnMouseUp(MouseButton button) override;
  void OnMouseMove(const glm::vec2 &position) override;

private:
  SplitOrientation m_Orientation;
  float m_SplitRatio;  // 0.0 to 1.0
  float m_DividerSize; // Logical pixels
  bool m_IsDragging;
  float m_DragStartRatio;
  glm::vec2 m_DragStartPos;

  UIControlPtr m_Pane1; // Left/Top pane
  UIControlPtr m_Pane2; // Right/Bottom pane

  std::function<void(float)> m_OnSplitChanged;

  // Get divider rect in local coordinates (logical)
  glm::vec4 GetDividerRect() const;

  // Layout the two panes based on current split ratio
  void LayoutPanes();
};

} // namespace Vivid
