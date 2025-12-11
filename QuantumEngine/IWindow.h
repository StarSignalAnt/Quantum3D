#pragma once

#include "DockTypes.h"
#include "UIControl.h"
#include <functional>
#include <memory>
#include <vector>

namespace Vivid {

class IWindow : public UIControl {
public:
  IWindow(const std::string &title = "Window");
  virtual ~IWindow() = default;

  // Window management
  void AddTab(std::shared_ptr<IWindow> window, bool makeActive = false);
  void SetDocked(bool docked) { m_IsDocked = docked; }
  bool IsDocked() const { return m_IsDocked; }

  // Logic
  void Update(float deltaTime) override;

  // Helpers
  // Returns x, y, width, height
  glm::vec4 GetTitleBarRect() const;
  glm::vec4 GetClientArea() const;

  // Tab Tearing
  void DetachTab(int tabIndex);
  void StartDrag(const glm::vec2 &globalMouse); // Helper to force start drag

  // Overrides for Content Management
  void AddChild(UIControlPtr child) override;
  void RemoveChild(UIControlPtr child) override;
  const std::vector<UIControlPtr> &GetContentControls() const;

  // Override to check resize before children process input
  bool ProcessInput(const AppInput &input, const glm::vec2 &mousePos,
                    UIControl *&outCapturedControl) override;

  // Override for passive input handling (mouse wheel scrolling)
  bool ProcessPassiveInput(const AppInput &input,
                           const glm::vec2 &mousePos) override;

  // Override GetMinSize to include TitleBar + Content requirements
  glm::vec2 GetMinSize() const override;

  // --- Docking Infrastructure ---

  // Dock zone this window is in (None = floating)
  void SetDockZone(DockZone zone) { m_DockZone = zone; }
  DockZone GetDockZone() const { return m_DockZone; }

  // Undock from current container and float
  void Undock();

  // Close callback (for dock containers to handle window close)
  void SetOnCloseCallback(std::function<void(IWindow *)> callback) {
    m_OnCloseCallback = callback;
  }
  void Close(); // Trigger close (calls callback if set)

  // Get dock hints for this window (where can other windows dock)
  std::vector<DockHint> GetDockHints(const glm::vec2 &mousePos) const;

protected:
  void Draw(Draw2D *draw2D) override; // Override to manage client area scissor
  void OnDraw(Draw2D *draw2D) override;
  void OnMouseDown(MouseButton button) override;
  void OnMouseUp(MouseButton button) override;
  void OnMouseMove(const glm::vec2 &position) override;

  // Tabs
  std::vector<IWindow *>
      m_Tabs; // If empty, standard window. If set, first is usually 'this'.
  UIControlPtr m_ContentRoot; // Container for client area content
  int m_ActiveTabIndex;
  float m_TabHeaderHeight;

  // State
  bool m_IsDragging;
  bool m_IsResizing;
  bool m_IsDocked; // If true, window is docked to IDock
  bool m_IsTab;    // If true, window is a tab inside another window (skip title
                   // bar)

  // Tab Dragging / Tearing
  bool m_IsDraggingTab; // Mouse down on tab, potentially dragging
  bool m_DragTearsTab; // If true, dragging will detach. If false, moves window.
  bool m_IsProxyDragging;     // We have detached a tab and are managing it
  IWindow *m_ProxyDragWindow; // The detached window we are controlling
  float m_TabDragOffsetX;     // Offset from tab left edge to mouse click x

  // Docking
  IWindow *
      m_PotentialDockTarget; // Valid target we are hovering over while dragging
  DockZone m_DockZone;       // Zone this window is docked in
  std::function<void(IWindow *)> m_OnCloseCallback;

  glm::vec2 m_DragStartPos;    // Mouse pos when drag started
  glm::vec2 m_WindowStartPos;  // Window pos when drag started
  glm::vec2 m_ResizeStartSize; // Window size when resize started
  glm::vec2 m_MouseStartPos;   // Global mouse pos at start of drag

  float m_TitleBarHeight;

  // Internal helper to draw tabs
  void DrawTabs(Draw2D *draw2D);
  bool CheckTabClick(const glm::vec2 &mousePos);

  // Scrollers
  // Scrollers
  class IVerticalScroller *m_VScroller;
  class IHorizontalScroller *m_HScroller;
  glm::vec2 m_ScrollOffset;

  // Drag notification helpers (for docking preview)
  void NotifyDragStart(const glm::vec2 &globalMouse);
  void NotifyDragEnd();
};

} // namespace Vivid
