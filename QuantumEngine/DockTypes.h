#pragma once

#include "glm/glm.hpp"
#include <memory>

namespace Vivid {

// Forward declarations
class IWindow;
class UIControl;
struct DockLayoutNode;

// Dock zones for docking windows to edges or center (tabs)
enum class DockZone {
  None,   // Not docked / floating
  Left,   // Docked to left edge
  Right,  // Docked to right edge
  Top,    // Docked to top edge
  Bottom, // Docked to bottom edge
  Center  // Docked as tab (merged with existing window)
};

// Splitter orientation
enum class SplitOrientation {
  Horizontal, // Split left/right
  Vertical    // Split top/bottom
};

// Hint shown when dragging a window over a potential dock target
struct DockHint {
  DockZone Zone = DockZone::None;
  IWindow *TargetWindow = nullptr; // Window to dock into/beside
  std::shared_ptr<DockLayoutNode> TargetNode = nullptr; // Specific node target
  glm::vec4 PreviewRect = glm::vec4(0.0f); // x, y, width, height of preview
  bool IsValid = false;
};

// Drag operation state for tracking active drag
struct DragOperation {
  UIControl *DragSource = nullptr;  // Control being dragged
  IWindow *DraggedWindow = nullptr; // Window being dragged (if applicable)
  glm::vec2 DragStartPos = glm::vec2(0.0f);
  glm::vec2 DragOffset = glm::vec2(0.0f); // Offset from control origin to mouse
  bool IsActive = false;
};

} // namespace Vivid
