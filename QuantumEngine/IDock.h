#pragma once

#include "DockTypes.h"
#include "UIControl.h"
#include <functional>
#include <memory>
#include <vector>

namespace Vivid {

class AppUI;
class Draw2D;
class IWindow;
class AppUI;
class Draw2D;
class IWindow;
class DockLayout;
struct DockLayoutNode;

// IDock: The main docking container that manages dockable windows
// This control fills the entire screen and serves as the root for all
// dockable content. Windows can be docked to edges (Left/Right/Top/Bottom)
// or as tabs (Center).
class IDock : public UIControl {
public:
  IDock();
  virtual ~IDock() = default;

  // Dock a window to a specific zone
  void DockWindow(std::shared_ptr<IWindow> window, DockZone zone,
                  IWindow *relativeTo = nullptr);

  // Undock a window (make it floating)
  void UndockWindow(IWindow *window);

  // Get all docked windows (recursively builds list)
  std::vector<std::shared_ptr<IWindow>> GetDockedWindows() const;

  // Get the central content area (where Center-docked windows go)
  UIControlPtr
  GetCenterPane() const; // TODO: Check if still relevant with full BSP

  // Update and rendering
  void Update(float deltaTime) override;

  // Override to check for dock zones when dragging
  bool ProcessInput(const AppInput &input, const glm::vec2 &mousePos,
                    UIControl *&outCapturedControl) override;

  // Set reference to AppUI (for accessing screen size, drag state, etc.)
  void SetAppUI(AppUI *appUI);
  AppUI *GetAppUI() const { return m_AppUI; }

  // Dock preview (called during drag to show where window will dock)
  void UpdateDockPreview(const glm::vec2 &mousePos, IWindow *draggedWindow);
  void ClearDockPreview();

  // Get current preview state
  bool IsShowingPreview() const { return m_ShowingPreview; }
  const DockHint &GetCurrentPreview() const { return m_CurrentPreview; }

  // Get active dock hint at mouse position
  DockHint GetDockHintAtPosition(const glm::vec2 &mousePos,
                                 IWindow *draggedWindow) const;

protected:
  void OnDraw(Draw2D *draw2D) override;
  // Handle resizing input
  void OnMouseDown(MouseButton button) override;
  void OnMouseUp(MouseButton button) override;
  void OnMouseMove(const glm::vec2 &position) override;

private:
  AppUI *m_AppUI;

  // Root of the Dock Layout
  DockLayout *m_Layout;

  // Dock Preview State
  bool m_ShowingPreview;
  DockHint m_CurrentPreview;

  // Resizing State
  bool m_IsResizing = false;
  std::shared_ptr<DockLayoutNode> m_ResizingNode;
  glm::vec2 m_ResizeStartPos;
  float m_StartResizeRatio = 0.5f;

  // Recursive helpers
  void UpdateNodeStart(std::shared_ptr<DockLayoutNode> node, float deltaTime);
  void CollectWindows(std::shared_ptr<DockLayoutNode> node,
                      std::vector<std::shared_ptr<IWindow>> &list) const;

  // Dock zone rectangles (for hit testing)
  // These might be dynamic now based on the node under mouse

  // Debugging
  bool m_DebugLayout = false;
  void DrawLayoutDebug(Draw2D *draw2D);
  void DrawNodeDebug(Draw2D *draw2D, std::shared_ptr<DockLayoutNode> node,
                     int &counter);
};

} // namespace Vivid
