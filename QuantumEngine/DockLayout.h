#pragma once

#include "DockTypes.h"
#include "glm/glm.hpp"
#include <memory>
#include <vector>

namespace Vivid {

enum class DockLayoutNodeType {
  Empty, // Represents empty space (background)
  Leaf,  // Represents a docked window
  Split  // Represents a divided area
};

struct DockLayoutNode {
  DockLayoutNodeType Type = DockLayoutNodeType::Empty;
  std::shared_ptr<DockLayoutNode> Parent;
  std::shared_ptr<DockLayoutNode> Child1;
  std::shared_ptr<DockLayoutNode> Child2;
  SplitOrientation Orientation = SplitOrientation::Horizontal;
  float SplitRatio = 0.5f; // 0.0 to 1.0
  void *Content =
      nullptr; // Pointer to the window (owner responsible for casting)
  glm::vec4 Bounds = glm::vec4(0.0f); // Calculated screen rect

  bool IsSplit() const { return Type == DockLayoutNodeType::Split; }
  bool IsLeaf() const { return Type == DockLayoutNodeType::Leaf; }
  bool IsEmpty() const { return Type == DockLayoutNodeType::Empty; }
};

class DockLayout {
public:
  DockLayout();
  ~DockLayout() = default;

  // Set the total available area for the layout
  void SetBounds(const glm::vec4 &bounds);

  // Dock a window.
  // window: The window object (opaque pointer)
  // zone: Where to dock
  // targetWindow: If docking relative to another window, provide it here.
  //               If null, tries to dock to root or creates new split.
  //               If null, tries to dock to root or creates new split.
  void Dock(void *window, DockZone zone, void *targetWindow = nullptr);

  // Dock directly to a specific node (bypassing window lookup)
  void DockToNode(std::shared_ptr<DockLayoutNode> node, void *window,
                  DockZone zone);

  // Resize a split node and smart-adjust children (consume empty space)
  void ResizeNode(std::shared_ptr<DockLayoutNode> node, float newRatio);

  // Undock a window.
  void Undock(void *window);

  // Update layout calculations (recursive)
  void RecalculateLayout();

  // Find the node containing the specific window
  std::shared_ptr<DockLayoutNode> FindNode(void *window);

  // Find leaf node at pixel position (for hitting empty space or windows)
  std::shared_ptr<DockLayoutNode> FindNodeAt(const glm::vec2 &pos);

  std::shared_ptr<DockLayoutNode> GetRoot() const { return m_Root; }

  // Check if resize is happening at this position (returns split node if so)
  std::shared_ptr<DockLayoutNode> CheckResizeHit(const glm::vec2 &pos,
                                                 float thickness = 7.0f);
  void ResizeNode(std::shared_ptr<DockLayoutNode> node,
                  const glm::vec2 &mousePos);

private:
  std::shared_ptr<DockLayoutNode> m_Root;
  glm::vec4 m_Bounds; // Total area

  void RecalculateNode(std::shared_ptr<DockLayoutNode> node,
                       const glm::vec4 &bounds);
  std::shared_ptr<DockLayoutNode>
  FindNodeRecursive(std::shared_ptr<DockLayoutNode> node, void *window);
  std::shared_ptr<DockLayoutNode>
  FindNodeAtRecursive(std::shared_ptr<DockLayoutNode> node,
                      const glm::vec2 &pos);

  // Helper to replace a child in its parent
  void ReplaceChild(std::shared_ptr<DockLayoutNode> parent,
                    std::shared_ptr<DockLayoutNode> oldChild,
                    std::shared_ptr<DockLayoutNode> newChild);

  // Helper to clean up splits that have empty/invalid children (merging)
  void SimplifyTree(std::shared_ptr<DockLayoutNode> node);

public:
  void PrintTree();

private:
  void PrintNode(std::shared_ptr<DockLayoutNode> node, int indent);
};

} // namespace Vivid
