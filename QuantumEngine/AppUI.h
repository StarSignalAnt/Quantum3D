#pragma once

#include "DockTypes.h"
#include "Draw2D.h"
#include "ThemeDarkUI.h"
#include "UIControl.h"
#include "UITheme.h"
#include <memory>

namespace Vivid {

class VividDevice;
class IWindow;
class IDock;

class AppUI {
public:
  AppUI();
  ~AppUI();

  // Initialize with Draw2D and device for rendering
  void Init(Draw2D *draw2D, VividDevice *device);

  // Root control - all UI is a child of this
  UIControlPtr GetRoot() const { return m_Root; }

  // Theme
  UITheme *GetTheme() const { return m_CurrentTheme; }
  void SetTheme(UITheme *theme);

  // Convenience methods to add/remove from root
  void AddControl(UIControlPtr control);
  void RemoveControl(UIControlPtr control);
  void Clear();

  // Hit Testing
  UIControl *GetControlAt(const glm::vec2 &mousePos);

  // Process input for all UI
  void ProcessInput(const AppInput &input);

  // Update all UI
  void Update(float deltaTime);

  // Render all UI
  void Render();

  void SetScreenSize(float width, float height);
  glm::vec2 GetScreenSize() const { return m_ScreenSize; }

  // Global DPI Scale
  static float GetScale() { return s_GlobalScale; }
  static void SetScale(float scale) { s_GlobalScale = scale; }

  // Drag operation tracking (for docking)
  void StartDragOperation(IWindow *window, const glm::vec2 &startPos,
                          const glm::vec2 &offset);
  void EndDragOperation();
  bool IsDraggingWindow() const { return m_DragOp.IsActive; }
  IWindow *GetDraggedWindow() const { return m_DragOp.DraggedWindow; }
  const DragOperation &GetDragOperation() const { return m_DragOp; }

  // Dock preview (visual hint where window will dock)
  void SetDockPreview(const DockHint &hint);
  void ClearDockPreview();
  bool HasDockPreview() const { return m_DockPreview.IsValid; }
  const DockHint &GetDockPreview() const { return m_DockPreview; }

  // Find dock hints at mouse position (what zones are available)
  DockHint GetDockHintAt(const glm::vec2 &mousePos, IWindow *draggedWindow);

  // Main dock container - provides the docking system
  void SetDock(std::shared_ptr<IDock> dock);
  std::shared_ptr<IDock> GetDock() const { return m_Dock; }

  // Create and add a default dock that fills the screen
  std::shared_ptr<IDock> CreateDock();

private:
  static float s_GlobalScale;
  UIControlPtr m_Root;
  UIControlPtr m_CapturedControl; // Control that has captured the mouse
  Draw2D *m_Draw2DPtr;
  UITheme *m_CurrentTheme;
  glm::vec2 m_ScreenSize;
  glm::vec2 m_LastMousePos;

  // Docking infrastructure
  DragOperation m_DragOp;
  DockHint m_DockPreview;
  std::shared_ptr<IDock> m_Dock;
};

} // namespace Vivid
