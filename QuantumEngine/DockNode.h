#pragma once

#include "DockTypes.h"
#include <memory>
#include <vector>

namespace Vivid {

class IWindow;
class Draw2D;

enum class DockNodeType { Leaf, Split };

class DockNode : public std::enable_shared_from_this<DockNode> {
public:
  DockNode();
  ~DockNode() = default;

  // Content management
  void SetWindow(std::shared_ptr<IWindow> window);
  std::shared_ptr<IWindow> GetWindow() const { return m_Window; }

  // Tree structure
  void SetSplit(SplitOrientation orientation, float ratio);
  void SetChildren(std::shared_ptr<DockNode> child1,
                   std::shared_ptr<DockNode> child2);

  DockNodeType GetType() const { return m_Type; }
  std::shared_ptr<DockNode> GetParent() const { return m_Parent.lock(); }
  std::shared_ptr<DockNode> GetChild1() const { return m_Child1; }
  std::shared_ptr<DockNode> GetChild2() const { return m_Child2; }

  SplitOrientation GetSplitOrientation() const { return m_SplitOrientation; }
  float GetSplitRatio() const { return m_SplitRatio; }
  void SetSplitRatio(float ratio) { m_SplitRatio = ratio; }

  // Layout
  void SetBounds(const glm::vec4 &bounds);
  const glm::vec4 &GetBounds() const { return m_Bounds; }

  // Update logic (updates children)
  void Update(float deltaTime);

  // Rendering (draws dividers, windows draw themselves but maybe we need to
  // debug draw)
  void Draw(Draw2D *draw2D);

  // Tree operations
  bool IsLeaf() const { return m_Type == DockNodeType::Leaf; }
  bool IsSplit() const { return m_Type == DockNodeType::Split; }

  // Helper to collect all windows
  void CollectWindows(std::vector<std::shared_ptr<IWindow>> &windows);

  // Find node containing point (for input/docking)
  std::shared_ptr<DockNode> FindNodeAt(const glm::vec2 &pos);

  // Clean empty nodes or merge if child is empty
  void Prune();

private:
  DockNodeType m_Type = DockNodeType::Leaf;
  std::weak_ptr<DockNode> m_Parent;

  // For Split Nodes
  std::shared_ptr<DockNode> m_Child1; // Left / Top
  std::shared_ptr<DockNode> m_Child2; // Right / Bottom
  SplitOrientation m_SplitOrientation = SplitOrientation::Horizontal;
  float m_SplitRatio = 0.5f;

  // For Leaf Nodes
  std::shared_ptr<IWindow> m_Window;

  // Bounds calculation
  glm::vec4 m_Bounds; // x, y, w, h
};

} // namespace Vivid
