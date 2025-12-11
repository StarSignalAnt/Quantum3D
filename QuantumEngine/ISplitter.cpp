#include "ISplitter.h"
#include "AppUI.h"
#include "Draw2D.h"
#include "UITheme.h"
#include "glm/glm.hpp"

namespace Vivid {

ISplitter::ISplitter(SplitOrientation orientation)
    : m_Orientation(orientation), m_SplitRatio(0.5f), m_DividerSize(6.0f),
      m_IsDragging(false), m_DragStartRatio(0.0f), m_DragStartPos(0.0f) {
  // Create the two panes as child containers
  m_Pane1 = std::make_shared<UIControl>();
  m_Pane2 = std::make_shared<UIControl>();

  // Add as direct children (bypass any override behavior)
  UIControl::AddChild(m_Pane1);
  UIControl::AddChild(m_Pane2);

  // Default size
  SetSize(glm::vec2(400.0f, 300.0f));
}

void ISplitter::SetSplitRatio(float ratio) {
  m_SplitRatio = glm::clamp(ratio, 0.05f, 0.95f);
  if (m_OnSplitChanged) {
    m_OnSplitChanged(m_SplitRatio);
  }
  LayoutPanes();
}

glm::vec4 ISplitter::GetDividerRect() const {
  float scale = AppUI::GetScale();
  glm::vec2 size = m_Size; // Logical size

  float dividerLogical = m_DividerSize;

  if (m_Orientation == SplitOrientation::Horizontal) {
    // Vertical divider (splits left/right)
    float splitX = size.x * m_SplitRatio - dividerLogical * 0.5f;
    return glm::vec4(splitX, 0.0f, dividerLogical, size.y);
  } else {
    // Horizontal divider (splits top/bottom)
    float splitY = size.y * m_SplitRatio - dividerLogical * 0.5f;
    return glm::vec4(0.0f, splitY, size.x, dividerLogical);
  }
}

// Helper to find effective min size of a pane by checking its children
static glm::vec2 GetEffectiveMinSize(UIControlPtr pane) {
  if (!pane)
    return glm::vec2(0.0f);

  // If the pane itself has a min size, use it (e.g. if it's a specific control)
  glm::vec2 size = pane->GetMinSize();

  // Also check children (e.g. if pane is a container for a Window)
  for (auto &child : pane->GetChildren()) {
    glm::vec2 childMin = child->GetMinSize();
    size.x = std::max(size.x, childMin.x);
    size.y = std::max(size.y, childMin.y);
  }
  return size;
}

glm::vec2 ISplitter::GetMinSize() const {
  glm::vec2 min1 = GetEffectiveMinSize(m_Pane1);
  glm::vec2 min2 = GetEffectiveMinSize(m_Pane2);
  float div = m_DividerSize; // Logical

  if (m_Orientation == SplitOrientation::Horizontal) {
    return glm::vec2(min1.x + min2.x + div, std::max(min1.y, min2.y));
  } else {
    return glm::vec2(std::max(min1.x, min2.x), min1.y + min2.y + div);
  }
}

void ISplitter::LayoutPanes() {
  glm::vec2 size = m_Size; // Logical
  float dividerLogical = m_DividerSize;

  if (m_Orientation == SplitOrientation::Horizontal) {
    // Pane1: Left, Pane2: Right
    float pane1Width = size.x * m_SplitRatio - dividerLogical * 0.5f;
    float pane2Width = size.x * (1.0f - m_SplitRatio) - dividerLogical * 0.5f;

    // Respect min sizes recursively
    glm::vec2 minSize1 = GetEffectiveMinSize(m_Pane1);
    glm::vec2 minSize2 = GetEffectiveMinSize(m_Pane2);

    // Clamp Pane 1 Min
    if (pane1Width < minSize1.x) {
      pane1Width = minSize1.x;
      // Recalculate Pane 2
      pane2Width = size.x - pane1Width - dividerLogical;
    }

    // Clamp Pane 2 Min (if Pane 1 didn't already eat everything)
    if (pane2Width < minSize2.x) {
      pane2Width = minSize2.x;
      // Recalculate Pane 1
      pane1Width = size.x - pane2Width - dividerLogical;

      // Re-check Pane 1 Min (Conflict resolution: usually prefer Pane 1 or
      // 50/50, but let's just clamp)
      if (pane1Width < minSize1.x) {
        // Both are too big?
        // Fallback: Just split available space or let them clip?
        // Let's stick to min1 constraint if possible, but min2 usually wins if
        // it's the specific window
      }
    }

    // Ensure we don't go negative
    pane1Width = std::max(0.0f, pane1Width);
    pane2Width = std::max(0.0f, pane2Width);

    m_Pane1->SetPosition(glm::vec2(0.0f, 0.0f));
    m_Pane1->SetSize(glm::vec2(pane1Width, size.y));

    m_Pane2->SetPosition(glm::vec2(pane1Width + dividerLogical, 0.0f));
    m_Pane2->SetSize(glm::vec2(pane2Width, size.y));
  } else {
    // Pane1: Top, Pane2: Bottom
    float pane1Height = size.y * m_SplitRatio - dividerLogical * 0.5f;
    float pane2Height = size.y * (1.0f - m_SplitRatio) - dividerLogical * 0.5f;

    // Respect min sizes recursively
    glm::vec2 minSize1 = GetEffectiveMinSize(m_Pane1);
    glm::vec2 minSize2 = GetEffectiveMinSize(m_Pane2);

    // Clamp Pane 1 Min
    if (pane1Height < minSize1.y) {
      pane1Height = minSize1.y;
      pane2Height = size.y - pane1Height - dividerLogical;
    }

    // Clamp Pane 2 Min
    if (pane2Height < minSize2.y) {
      pane2Height = minSize2.y;
      pane1Height = size.y - pane2Height - dividerLogical;
    }

    // Ensure we don't go negative
    pane1Height = std::max(0.0f, pane1Height);
    pane2Height = std::max(0.0f, pane2Height);

    m_Pane1->SetPosition(glm::vec2(0.0f, 0.0f));
    m_Pane1->SetSize(glm::vec2(size.x, pane1Height));

    m_Pane2->SetPosition(glm::vec2(0.0f, pane1Height + dividerLogical));
    m_Pane2->SetSize(glm::vec2(size.x, pane2Height));
  }

  // FORCE: Resize children of panes to fill the pane
  // This is critical for nested layouts
  for (auto &child : m_Pane1->GetChildren()) {
    child->SetPosition(glm::vec2(0.0f, 0.0f));
    child->SetSize(m_Pane1->GetSize());
  }
  for (auto &child : m_Pane2->GetChildren()) {
    child->SetPosition(glm::vec2(0.0f, 0.0f));
    child->SetSize(m_Pane2->GetSize());
  }
}

void ISplitter::Update(float deltaTime) {
  LayoutPanes();
  UIControl::Update(deltaTime);
}

void ISplitter::OnDraw(Draw2D *draw2D) {
  if (!draw2D)
    return;

  UITheme *theme = GetTheme();
  if (!theme)
    return;

  Texture2D *whiteTex = theme->GetWhiteTexture();
  if (!whiteTex)
    return;

  float scale = AppUI::GetScale();
  glm::vec2 absPos = GetAbsolutePosition();

  // Get divider rect in logical, convert to pixels for drawing
  glm::vec4 divRect = GetDividerRect();
  glm::vec2 divPos = absPos + glm::vec2(divRect.x, divRect.y) * scale;
  glm::vec2 divSize = glm::vec2(divRect.z, divRect.w) * scale;

  // Divider color: darker when idle, lighter when dragging
  glm::vec4 divColor = m_IsDragging ? glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)
                                    : glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);

  draw2D->DrawTexture(divPos, divSize, whiteTex, divColor, BlendMode::Solid);
}

void ISplitter::OnMouseDown(MouseButton button) {
  if (button == MouseButton::Left) {
    // Check if click is on divider
    glm::vec4 divRect = GetDividerRect();

    // We'll use the drag state to update ratio based on mouse movement
    // For simplicity, if we get a mouse down and we're hovered, start drag
    m_IsDragging = true;
    m_DragStartRatio = m_SplitRatio;
    m_DragStartPos = GetAbsolutePosition();
  }
}

void ISplitter::OnMouseUp(MouseButton button) {
  if (button == MouseButton::Left) {
    m_IsDragging = false;
  }
}

void ISplitter::OnMouseMove(const glm::vec2 &position) {
  if (m_IsDragging) {
    float scale = AppUI::GetScale();
    glm::vec2 size = m_Size;

    // position is local (in logical coordinates after scale conversion)
    if (m_Orientation == SplitOrientation::Horizontal) {
      // Calculate new ratio based on mouse X position
      float mouseX = position.x; // Already logical
      float newRatio = mouseX / size.x;
      SetSplitRatio(newRatio);
    } else {
      // Calculate new ratio based on mouse Y position
      float mouseY = position.y;
      float newRatio = mouseY / size.y;
      SetSplitRatio(newRatio);
    }
  }
}

} // namespace Vivid
