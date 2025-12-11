#include "UIControl.h"
#include "AppUI.h"
#include "Draw2D.h"
#include <algorithm>

// #include <cstdio> // Debug prints removed for cleanliness

namespace Vivid {

UIControl::UIControl()
    : m_Position(0.0f), m_Size(100.0f, 30.0f), m_Color(0.3f, 0.3f, 0.3f, 1.0f),
      m_Visible(true), m_Enabled(true), m_Hovered(false), m_Focused(false),
      m_Parent(nullptr), m_Theme(nullptr), m_WasMouseDown(false),
      m_ClipsChildren(false), m_MinSize(0.0f), m_MaxSize(0.0f) {}

glm::vec2 UIControl::GetSize() const { return m_Size * AppUI::GetScale(); }

glm::vec2 UIControl::GetAbsolutePosition() const {
  float scale = AppUI::GetScale();
  if (m_Parent) {
    glm::vec2 parentAbs = m_Parent->GetAbsolutePosition();
    return parentAbs + (m_Position * scale);
  }
  return m_Position * scale;
}

glm::vec4 UIControl::GetClipRect() const {
  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 size = GetSize();
  return glm::vec4(absPos.x, absPos.y, size.x, size.y);
}

void UIControl::AddChild(UIControlPtr child) {
  if (child) {
    child->m_Parent = this;
    m_Children.push_back(child);
  }
}

void UIControl::RemoveChild(UIControlPtr child) {
  auto it = std::find(m_Children.begin(), m_Children.end(), child);
  if (it != m_Children.end()) {
    (*it)->m_Parent = nullptr;
    m_Children.erase(it);
  }
}

void UIControl::MoveChildToFront(UIControl *child) {
  if (!child)
    return;
  // Find the shared_ptr that matches this pointer
  auto it =
      std::find_if(m_Children.begin(), m_Children.end(),
                   [child](const UIControlPtr &p) { return p.get() == child; });

  if (it != m_Children.end() && it != m_Children.end() - 1) {
    // Rotate the found element to the end of the vector (renders last = top)
    std::rotate(it, it + 1, m_Children.end());
  }
}

void UIControl::MoveChildToBack(UIControl *child) {
  if (!child)
    return;
  auto it =
      std::find_if(m_Children.begin(), m_Children.end(),
                   [child](const UIControlPtr &p) { return p.get() == child; });

  if (it != m_Children.end() && it != m_Children.begin()) {
    // Rotate the found element to the beginning (renders first = bottom)
    std::rotate(m_Children.begin(), it, it + 1);
  }
}

void UIControl::BringToFront() {
  if (m_Parent) {
    m_Parent->MoveChildToFront(this);
  }
}

void UIControl::SendToBack() {
  if (m_Parent) {
    m_Parent->MoveChildToBack(this);
  }
}

int UIControl::GetZOrder() const {
  if (!m_Parent)
    return -1;
  const auto &siblings = m_Parent->m_Children;
  for (size_t i = 0; i < siblings.size(); ++i) {
    if (siblings[i].get() == this) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void UIControl::ClearChildren() {
  for (auto &child : m_Children) {
    child->m_Parent = nullptr;
  }
  m_Children.clear();
}

void UIControl::ClearHoverState() {
  if (m_Hovered) {
    m_Hovered = false;
    OnMouseLeave();
  }
  for (auto &child : m_Children) {
    child->ClearHoverState();
  }
}

bool UIControl::Contains(const glm::vec2 &point) const {
  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 size = GetSize(); // Scaled Size
  return point.x >= absPos.x && point.x < absPos.x + size.x &&
         point.y >= absPos.y && point.y < absPos.y + size.y;
}

UIControl *UIControl::GetControlAt(const glm::vec2 &point, UIControl *exclude) {
  if (!m_Visible || !m_Enabled)
    return nullptr;
  if (this == exclude)
    return nullptr;

  // Check children in reverse order (top first)
  for (auto it = m_Children.rbegin(); it != m_Children.rend(); ++it) {
    if (it->get() == exclude)
      continue;

    UIControl *hit = (*it)->GetControlAt(point, exclude);
    if (hit)
      return hit;
  }

  if (Contains(point)) {
    return this;
  }
  return nullptr;
}

void UIControl::Update(float deltaTime) {
  if (!m_Visible)
    return;

  // Update children
  for (auto &child : m_Children) {
    child->Update(deltaTime);
  }
}

void UIControl::Draw(Draw2D *draw2D) {
  if (!m_Visible)
    return;

  // Draw self
  OnDraw(draw2D);

  // Apply scissor clipping for children if enabled
  if (m_ClipsChildren && draw2D) {
    draw2D->PushScissor(GetClipRect());
  }

  // Draw children
  for (auto &child : m_Children) {
    child->Draw(draw2D);
  }

  // Restore scissor
  if (m_ClipsChildren && draw2D) {
    draw2D->PopScissor();
  }
}

bool UIControl::ProcessInput(const AppInput &input, const glm::vec2 &mousePos,
                             UIControl *&outCapturedControl) {
  if (!m_Visible || !m_Enabled)
    return false;

  // Process children first (top-most/last added gets priority)
  for (auto it = m_Children.rbegin(); it != m_Children.rend(); ++it) {
    if ((*it)->ProcessInput(input, mousePos, outCapturedControl)) {
      return true;
    }
  }

  // Check if mouse is over this control
  bool isOver = Contains(mousePos);

  // Handle mouse enter/leave
  if (isOver && !m_Hovered) {
    m_Hovered = true;
    OnMouseEnter();
  } else if (!isOver && m_Hovered) {
    m_Hovered = false;
    OnMouseLeave();
  }

  // Handle mouse movement
  if (isOver) {
    OnMouseMove(mousePos - GetAbsolutePosition());
  }

  // Handle mouse buttons
  bool mouseDown = input.IsMouseButtonDown(MouseButton::Left);

  if (isOver) {
    if (input.IsMouseButtonPressed(MouseButton::Left)) {
      OnMouseDown(MouseButton::Left);
      m_WasMouseDown = true;
      m_Focused = true;
      OnFocusGained();
      // Notify capture
      outCapturedControl = this;
    }
    if (input.IsMouseButtonPressed(MouseButton::Right)) {
      OnMouseDown(MouseButton::Right);
    }
    if (input.IsMouseButtonPressed(MouseButton::Middle)) {
      OnMouseDown(MouseButton::Middle);
    }

    if (input.IsMouseButtonReleased(MouseButton::Left)) {
      OnMouseUp(MouseButton::Left);
      if (m_WasMouseDown) {
        OnClick();
      }
      m_WasMouseDown = false;
    }
    if (input.IsMouseButtonReleased(MouseButton::Right)) {
      OnMouseUp(MouseButton::Right);
    }
    if (input.IsMouseButtonReleased(MouseButton::Middle)) {
      OnMouseUp(MouseButton::Middle);
    }

    return true; // Input was handled
  } else {
    // Lost focus if clicked elsewhere
    if (input.IsMouseButtonPressed(MouseButton::Left) && m_Focused) {
      m_Focused = false;
      OnFocusLost();
    }
    m_WasMouseDown = false;
  }

  // Handle keyboard input if focused
  if (m_Focused) {
    for (int k = static_cast<int>(Key::Space); k <= static_cast<int>(Key::Menu);
         ++k) {
      Key key = static_cast<Key>(k);
      if (input.IsKeyPressed(key)) {
        OnKeyDown(key);
      }
      if (input.IsKeyReleased(key)) {
        OnKeyUp(key);
      }
    }
  }

  return false;
}

bool UIControl::ProcessPassiveInput(const AppInput &input,
                                    const glm::vec2 &mousePos) {
  if (!m_Visible || !m_Enabled)
    return false;

  // Process children first (top-most/last added gets priority)
  for (auto it = m_Children.rbegin(); it != m_Children.rend(); ++it) {
    if ((*it)->ProcessPassiveInput(input, mousePos)) {
      return true;
    }
  }

  // Base implementation does nothing - derived classes override for specific
  // passive input handling (like mouse wheel scrolling in IWindow)
  return false;
}

} // namespace Vivid
