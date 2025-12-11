#include "DockLayout.h"
#include <algorithm>
#include <iostream>

namespace Vivid {

void DockLayout::PrintTree() {
  std::cout << "=== Dock Layout Tree ===" << std::endl;
  PrintNode(m_Root, 0);
  std::cout << "========================" << std::endl;
}

void DockLayout::PrintNode(std::shared_ptr<DockLayoutNode> node, int indent) {
  if (!node)
    return;

  std::cout << std::string(indent * 2, ' ');
  if (node->IsEmpty())
    std::cout << "[Empty]";
  else if (node->IsLeaf())
    std::cout << "[Leaf " << node->Content << "]";
  else
    std::cout << "[Split "
              << (node->Orientation == SplitOrientation::Horizontal ? "H" : "V")
              << " " << node->SplitRatio << "]";

  std::cout << " Bounds: " << node->Bounds.x << "," << node->Bounds.y << " "
            << node->Bounds.z << "x" << node->Bounds.w << std::endl;

  if (node->IsSplit()) {
    PrintNode(node->Child1, indent + 1);
    PrintNode(node->Child2, indent + 1);
  }
}

DockLayout::DockLayout() {
  // Start with a single clean empty root
  m_Root = std::make_shared<DockLayoutNode>();
  m_Root->Type = DockLayoutNodeType::Empty;
}

void DockLayout::SetBounds(const glm::vec4 &bounds) {
  m_Bounds = bounds;
  RecalculateLayout();
}

void DockLayout::RecalculateLayout() {
  if (m_Root) {
    RecalculateNode(m_Root, m_Bounds);
  }
}

void DockLayout::RecalculateNode(std::shared_ptr<DockLayoutNode> node,
                                 const glm::vec4 &bounds) {
  if (!node)
    return;

  node->Bounds = bounds;

  if (node->IsSplit()) {
    float w = bounds.z;
    float h = bounds.w;
    float x = bounds.x;
    float y = bounds.y;

    glm::vec4 b1, b2;

    if (node->Orientation == SplitOrientation::Horizontal) {
      float splitW = w * node->SplitRatio;
      b1 = glm::vec4(x, y, splitW, h);
      b2 = glm::vec4(x + splitW, y, w - splitW, h);
    } else {
      float splitH = h * node->SplitRatio;
      b1 = glm::vec4(x, y, w, splitH);
      b2 = glm::vec4(x, y + splitH, w, h - splitH);
    }

    RecalculateNode(node->Child1, b1);
    RecalculateNode(node->Child2, b2);
  }
}

void DockLayout::ResizeNode(std::shared_ptr<DockLayoutNode> node,
                            float newRatio) {
  if (!node || !node->IsSplit())
    return;

  float oldRatio = node->SplitRatio;
  node->SplitRatio = newRatio;

  // Smart Resize Logic: Check adjacent children for compressible empty space
  float totalSize = (node->Orientation == SplitOrientation::Horizontal)
                        ? node->Bounds.z
                        : node->Bounds.w;

  // Case 1: Shrinking/Growing Child 2 (Bottom/Right).
  // If Child 2 is Split(Empty, Content), we want Content to preserve size
  // (Empty shrinks/grows).
  if (node->Child2 && node->Child2->IsSplit() &&
      node->Child2->Orientation == node->Orientation) {
    if (node->Child2->Child1 && node->Child2->Child1->IsEmpty()) {
      float h2_old = totalSize * (1.0f - oldRatio);
      float h2_new = totalSize * (1.0f - newRatio);

      if (h2_new > 1.0f && h2_old > 1.0f) {
        float contentRatio = (1.0f - node->Child2->SplitRatio);
        float contentSize = h2_old * contentRatio;

        float reqContentRatio = contentSize / h2_new;
        // Clamp
        if (reqContentRatio > 1.0f)
          reqContentRatio = 1.0f;

        // New Split Ratio for Child 2 (Empty portion)
        node->Child2->SplitRatio = 1.0f - reqContentRatio;
      }
    }
  }

  // Case 2: Shrinking/Growing Child 1 (Top/Left).
  // If Child 1 is Split(Content, Empty), we want Content to preserve size.
  if (node->Child1 && node->Child1->IsSplit() &&
      node->Child1->Orientation == node->Orientation) {
    if (node->Child1->Child2 && node->Child1->Child2->IsEmpty()) {
      float h1_old = totalSize * oldRatio;
      float h1_new = totalSize * newRatio;

      if (h1_new > 1.0f && h1_old > 1.0f) {
        float contentRatio = node->Child1->SplitRatio;
        float contentSize = h1_old * contentRatio;

        float reqRatio = contentSize / h1_new;
        if (reqRatio > 1.0f)
          reqRatio = 1.0f;

        node->Child1->SplitRatio = reqRatio;
      }
    }
  }

  RecalculateLayout();
}

std::shared_ptr<DockLayoutNode> DockLayout::FindNode(void *window) {
  return FindNodeRecursive(m_Root, window);
}

std::shared_ptr<DockLayoutNode>
DockLayout::FindNodeRecursive(std::shared_ptr<DockLayoutNode> node,
                              void *window) {
  if (!node)
    return nullptr;

  if (node->IsLeaf() && node->Content == window) {
    return node;
  }

  if (node->IsSplit()) {
    auto found = FindNodeRecursive(node->Child1, window);
    if (found)
      return found;
    return FindNodeRecursive(node->Child2, window);
  }

  return nullptr;
}

void DockLayout::Dock(void *window, DockZone zone, void *targetWindow) {
  // 1. If window is already docked, ignore or undock first?
  // Assuming caller handles undock if needed, or we just move it.
  // For safety, let's just proceed assuming it's free.

  std::shared_ptr<DockLayoutNode> targetNode = nullptr;

  if (targetWindow) {
    targetNode = FindNode(targetWindow);
  }

  // If no target node found (global dock or not found), find a suitable default
  if (!targetNode) {
    // Logic found previously: Find first empty or root
    auto findEmpty = [&](auto &&self, std::shared_ptr<DockLayoutNode> n)
        -> std::shared_ptr<DockLayoutNode> {
      if (!n)
        return nullptr;
      if (n->IsEmpty())
        return n;
      if (n->IsSplit()) {
        auto c1 = self(self, n->Child1);
        if (c1)
          return c1;
        auto c2 = self(self, n->Child2);
        if (c2)
          return c2;
      }
      return nullptr;
    };

    auto emptyNode = findEmpty(findEmpty, m_Root);
    if (emptyNode) {
      targetNode = emptyNode;
    } else {
      targetNode = m_Root;
    }
  }

  DockToNode(targetNode, window, zone);
}

void DockLayout::DockToNode(std::shared_ptr<DockLayoutNode> targetNode,
                            void *window, DockZone zone) {
  if (!targetNode)
    return;

  // New Content Node
  auto newContent = std::make_shared<DockLayoutNode>();
  newContent->Type = DockLayoutNodeType::Leaf;
  newContent->Content = window;

  // Logic for Empty Nodes
  if (targetNode->IsEmpty()) {
    if (zone == DockZone::Center) {
      // Morph targetNode into Leaf
      targetNode->Type = DockLayoutNodeType::Leaf;
      targetNode->Content = window;
      RecalculateLayout();
      return;
    }

    // Edge Docking on Empty -> Create Split
    auto newEmpty = std::make_shared<DockLayoutNode>();
    newEmpty->Type = DockLayoutNodeType::Empty;

    targetNode->Type = DockLayoutNodeType::Split;
    // targetNode becomes the split

    if (zone == DockZone::Left || zone == DockZone::Right) {
      targetNode->Orientation = SplitOrientation::Horizontal;
    } else {
      targetNode->Orientation = SplitOrientation::Vertical;
    }

    float totalSize = (targetNode->Orientation == SplitOrientation::Horizontal)
                          ? targetNode->Bounds.z
                          : targetNode->Bounds.w;

    float globalDimension =
        (targetNode->Orientation == SplitOrientation::Horizontal) ? m_Bounds.z
                                                                  : m_Bounds.w;
    float desiredSize = globalDimension * 0.25f;

    float ratio = desiredSize / totalSize;
    // printf("DockToNode Empty: Zone=%d Total=%.2f Ratio=%.2f\n", (int)zone,
    // totalSize, ratio);

    if (ratio > 0.5f)
      ratio = 0.5f;
    if (ratio < 0.1f)
      ratio = 0.1f; // Safety clamp

    if (zone == DockZone::Left || zone == DockZone::Top) {
      targetNode->Child1 = newContent;
      targetNode->Child2 = newEmpty;
      targetNode->SplitRatio = ratio;
    } else {
      targetNode->Child1 = newEmpty;
      targetNode->Child2 = newContent;
      targetNode->SplitRatio = 1.0f - ratio;
    }

    newContent->Parent = targetNode;
    newEmpty->Parent = targetNode;

    RecalculateLayout();
    return;
  }

  // Docking relative to existing Content or Split
  auto parent = targetNode->Parent;
  auto newSplit = std::make_shared<DockLayoutNode>();
  newSplit->Type = DockLayoutNodeType::Split;
  newSplit->Parent = parent;

  if (zone == DockZone::Left || zone == DockZone::Right) {
    newSplit->Orientation = SplitOrientation::Horizontal;
  } else if (zone == DockZone::Top || zone == DockZone::Bottom) {
    newSplit->Orientation = SplitOrientation::Vertical;
  } else if (zone == DockZone::Center) {
    // Default to Right split 50/50 for Center on non-empty (TAB TODO)
    newSplit->Orientation = SplitOrientation::Horizontal;
    zone = DockZone::Right;
  }

  float totalSize = (newSplit->Orientation == SplitOrientation::Horizontal)
                        ? targetNode->Bounds.z
                        : targetNode->Bounds.w;

  float globalDimension =
      (newSplit->Orientation == SplitOrientation::Horizontal) ? m_Bounds.z
                                                              : m_Bounds.w;
  float desiredSize = globalDimension * 0.25f;

  float ratio = desiredSize / totalSize;
  // printf("DockToNode Split: Zone=%d Total=%.2f Ratio=%.2f\n", (int)zone,
  // totalSize, ratio);

  if (ratio > 0.5f)
    ratio = 0.5f;
  if (ratio < 0.1f)
    ratio = 0.1f;

  if (zone == DockZone::Left || zone == DockZone::Top) {
    newSplit->Child1 = newContent;
    newSplit->Child2 = targetNode; // Existing node pushed
    newSplit->SplitRatio = ratio;
  } else {
    newSplit->Child1 = targetNode;
    newSplit->Child2 = newContent;
    newSplit->SplitRatio = 1.0f - ratio;
  }

  newContent->Parent = newSplit;
  targetNode->Parent = newSplit; // Re-parent existing

  // Replace targetNode in the tree with newSplit
  if (parent) {
    ReplaceChild(parent, targetNode, newSplit);
  } else {
    // Target was root
    m_Root = newSplit;
  }

  RecalculateLayout();
  std::cout << "Docking Complete. New Tree:" << std::endl;
  PrintTree();
}

void DockLayout::Undock(void *window) {
  auto node = FindNode(window);
  if (!node)
    return;

  // To undock:
  // Convert this node to Empty. This preserves the Split structure so that
  // the sibling (e.g. Top dock) does not suddenly resize to fill the whole
  // area.
  node->Type = DockLayoutNodeType::Empty;
  node->Content = nullptr;

  // Only merge if both siblings are empty (SimplifyTree does this)
  if (m_Root) {
    SimplifyTree(m_Root);
    RecalculateLayout();
  }
}

void DockLayout::ReplaceChild(std::shared_ptr<DockLayoutNode> parent,
                              std::shared_ptr<DockLayoutNode> oldChild,
                              std::shared_ptr<DockLayoutNode> newChild) {
  if (!parent)
    return;
  if (parent->Child1 == oldChild)
    parent->Child1 = newChild;
  else if (parent->Child2 == oldChild)
    parent->Child2 = newChild;
}

void DockLayout::SimplifyTree(std::shared_ptr<DockLayoutNode> node) {
  // If a node is a Split, and both children are Empty -> Merge to Empty
  if (!node || !node->IsSplit())
    return;

  SimplifyTree(node->Child1);
  SimplifyTree(node->Child2);

  if (node->IsSplit()) {
    bool c1Empty = node->Child1->IsEmpty();
    bool c2Empty = node->Child2->IsEmpty();

    if (c1Empty && c2Empty) {
      node->Type = DockLayoutNodeType::Empty;
      node->Child1 = nullptr;
      node->Child2 = nullptr;
    }
  }
}

std::shared_ptr<DockLayoutNode> DockLayout::FindNodeAt(const glm::vec2 &pos) {
  return FindNodeAtRecursive(m_Root, pos);
}

std::shared_ptr<DockLayoutNode>
DockLayout::FindNodeAtRecursive(std::shared_ptr<DockLayoutNode> node,
                                const glm::vec2 &pos) {
  if (!node)
    return nullptr;

  // Check bounds
  if (pos.x < node->Bounds.x || pos.x > node->Bounds.x + node->Bounds.z ||
      pos.y < node->Bounds.y || pos.y > node->Bounds.y + node->Bounds.w) {
    return nullptr;
  }

  if (node->IsSplit()) {
    auto c1 = FindNodeAtRecursive(node->Child1, pos);
    if (c1)
      return c1;
    auto c2 = FindNodeAtRecursive(node->Child2, pos);
    if (c2)
      return c2;
    // If in split bounds but not in children? Shouldn't happen if full
    // coverage.
  }

  // Leaf or Empty
  return node;
}

std::shared_ptr<DockLayoutNode>
DockLayout::CheckResizeHit(const glm::vec2 &mousePos, float thickness) {
  // Traverse tree to find a split divider close to mousePos
  // We can just iterate (or use recursive helper)

  std::vector<std::shared_ptr<DockLayoutNode>> stack;
  if (m_Root)
    stack.push_back(m_Root);

  while (!stack.empty()) {
    auto node = stack.back();
    stack.pop_back();

    if (node->IsSplit()) {
      glm::vec4 b = node->Bounds;
      glm::vec4 divider;

      if (node->Orientation == SplitOrientation::Horizontal) {
        float splitX = b.x + b.z * node->SplitRatio;
        divider = glm::vec4(splitX - thickness / 2, b.y, thickness, b.w);
      } else {
        float splitY = b.y + b.w * node->SplitRatio;
        divider = glm::vec4(b.x, splitY - thickness / 2, b.z, thickness);
      }

      if (mousePos.x >= divider.x && mousePos.x <= divider.x + divider.z &&
          mousePos.y >= divider.y && mousePos.y <= divider.y + divider.w) {
        return node;
      }

      stack.push_back(node->Child1);
      stack.push_back(node->Child2);
    }
  }
  return nullptr;
}

void DockLayout::ResizeNode(std::shared_ptr<DockLayoutNode> node,
                            const glm::vec2 &mousePos) {
  if (!node || !node->IsSplit())
    return;

  glm::vec4 b = node->Bounds;
  // Relative position
  float val = (node->Orientation == SplitOrientation::Horizontal)
                  ? (mousePos.x - b.x)
                  : (mousePos.y - b.y);
  float size = (node->Orientation == SplitOrientation::Horizontal) ? b.z : b.w;

  float ratio = val / size;
  ratio = std::clamp(ratio, 0.1f, 0.9f);
  node->SplitRatio = ratio;

  RecalculateLayout();
}

} // namespace Vivid
