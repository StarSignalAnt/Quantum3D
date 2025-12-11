#include "DockNode.h"
#include "Draw2D.h"
#include "IWindow.h"
#include <algorithm>

namespace Vivid {

DockNode::DockNode() : m_Type(DockNodeType::Leaf), m_Bounds(0.0f) {}

void DockNode::SetWindow(std::shared_ptr<IWindow> window) {
  m_Window = window;
  m_Type = DockNodeType::Leaf;
  m_Child1 = nullptr;
  m_Child2 = nullptr;
}

void DockNode::SetSplit(SplitOrientation orientation, float ratio) {
  m_Type = DockNodeType::Split;
  m_SplitOrientation = orientation;
  m_SplitRatio = ratio;
  m_Window = nullptr;
}

void DockNode::SetChildren(std::shared_ptr<DockNode> child1,
                           std::shared_ptr<DockNode> child2) {
  m_Child1 = child1;
  m_Child2 = child2;

  if (m_Child1)
    m_Child1->m_Parent = weak_from_this();
  if (m_Child2)
    m_Child2->m_Parent = weak_from_this();
}

void DockNode::SetBounds(const glm::vec4 &bounds) {
  m_Bounds = bounds;

  if (m_Type == DockNodeType::Split) {
    float w = bounds.z;
    float h = bounds.w;
    float x = bounds.x;
    float y = bounds.y;

    glm::vec4 b1, b2;

    if (m_SplitOrientation == SplitOrientation::Horizontal) {
      // Split Left/Right
      float splitW = w * m_SplitRatio;
      b1 = glm::vec4(x, y, splitW, h);
      b2 = glm::vec4(x + splitW, y, w - splitW, h);
    } else {
      // Split Top/Bottom
      float splitH = h * m_SplitRatio;
      b1 = glm::vec4(x, y, w, splitH);
      b2 = glm::vec4(x, y + splitH, w, h - splitH);
    }

    if (m_Child1)
      m_Child1->SetBounds(b1);
    if (m_Child2)
      m_Child2->SetBounds(b2);
  } else if (m_Type == DockNodeType::Leaf && m_Window) {
    // Set window position and size
    // Windows usually have their own position logic, but in docking they follow
    // the node
    m_Window->SetPosition(glm::vec2(bounds.x, bounds.y));
    m_Window->SetSize(glm::vec2(bounds.z, bounds.w));
  }
}

void DockNode::Update(float deltaTime) {
  if (m_Type == DockNodeType::Split) {
    if (m_Child1)
      m_Child1->Update(deltaTime);
    if (m_Child2)
      m_Child2->Update(deltaTime);
  } else if (m_Window) {
    // Should window update be called here? Usually AppUI calls Update on all
    // controls. If IWindow is a UIControl child of AppUI, it might be updated
    // twice. However, docked windows might not be direct children of root
    // anymore. IDock should handle this. For now, let's assume IDock calls
    // update on the tree.
    m_Window->Update(deltaTime);
  }
}

void DockNode::Draw(Draw2D *draw2D) {
  if (m_Type == DockNodeType::Split) {
    if (m_Child1)
      m_Child1->Draw(draw2D);
    if (m_Child2)
      m_Child2->Draw(draw2D);

    // Optional: Draw splitter bar visual
  } else if (m_Window) {
    // Window draws itself usually?
    // If IDock is drawing, we might need to tell window to draw.
    // But IWindow::OnDraw is protected.
    // We need to verify how windows are rendered.
    // Typically UIControl system renders children.
  }
}

void DockNode::CollectWindows(std::vector<std::shared_ptr<IWindow>> &windows) {
  if (m_Type == DockNodeType::Leaf && m_Window) {
    windows.push_back(m_Window);
  } else if (m_Type == DockNodeType::Split) {
    if (m_Child1)
      m_Child1->CollectWindows(windows);
    if (m_Child2)
      m_Child2->CollectWindows(windows);
  }
}

std::shared_ptr<DockNode> DockNode::FindNodeAt(const glm::vec2 &pos) {
  if (pos.x < m_Bounds.x || pos.x > m_Bounds.x + m_Bounds.z ||
      pos.y < m_Bounds.y || pos.y > m_Bounds.y + m_Bounds.w) {
    return nullptr;
  }

  if (m_Type == DockNodeType::Leaf) {
    return shared_from_this();
  }

  if (m_Child1) {
    auto node = m_Child1->FindNodeAt(pos);
    if (node)
      return node;
  }
  if (m_Child2) {
    auto node = m_Child2->FindNodeAt(pos);
    if (node)
      return node;
  }

  // Fallback if inside bounds but not in children (shouldn't happen in full
  // tree)
  return nullptr;
}

void DockNode::Prune() {
  // If we are a split, check children
  if (m_Type == DockNodeType::Split) {
    if (m_Child1)
      m_Child1->Prune();
    if (m_Child2)
      m_Child2->Prune();

    // If one child is missing or empty, replace self with other child
    // This is tricky because we need to modify our parent's pointer to us.
    // DockNode cannot easy replace itself in parent without parent's help.
    // We will handle pruning in IDock or a separate pass that returns the new
    // root.
  }
}

} // namespace Vivid
