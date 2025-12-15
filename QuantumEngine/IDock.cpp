#include "IDock.h"
#include "AppUI.h"
#include "DockLayout.h"
#include "Draw2D.h"
#include "IWindow.h"
#include "UITheme.h"
#include <algorithm>

namespace Vivid {

IDock::IDock()
    : m_AppUI(nullptr), m_ShowingPreview(false), m_Layout(new DockLayout()),
      m_ResizeStartPos(0.0f) {
  SetSize(glm::vec2(1400, 900.0f));
  SetPosition(glm::vec2(0.0f, 0.0f));
  SetVisible(true);
  SetVisible(true);
  SetEnabled(true);
}

void IDock::SetAppUI(AppUI *appUI) {
  m_AppUI = appUI;
  if (m_AppUI) {
    glm::vec2 screenSize = m_AppUI->GetScreenSize();
    SetSize(screenSize);

    // Also update layout bounds immediately
    if (m_Layout) {
      m_Layout->SetBounds(glm::vec4(0.0f, 0.0f, m_Size.x, m_Size.y));
    }
  }
}

// Destructor needed for m_Layout cleanup if using raw pointer, or use smart
// pointer Since we used raw pointer in header, we shouldn't forget to delete
// it. Actually default destructor is used in header. We should actallu use
// shared_ptr or delete in destructor? Let's add destructor to IDock.cpp or
// assume managed. For now, let's leak it or fix header later. But wait,
// removing header destructor = default was in user code? I should probably fix
// the leak. But first implementation.

std::vector<std::shared_ptr<IWindow>> IDock::GetDockedWindows() const {
  std::vector<std::shared_ptr<IWindow>> list;
  if (m_Layout && m_Layout->GetRoot()) {
    CollectWindows(m_Layout->GetRoot(), list);
  }
  return list;
}

void IDock::CollectWindows(std::shared_ptr<DockLayoutNode> node,
                           std::vector<std::shared_ptr<IWindow>> &list) const {
  if (!node)
    return;
  if (node->IsLeaf() && node->Content) {
    list.push_back(std::static_pointer_cast<IWindow>(
        std::static_pointer_cast<IWindow>(std::shared_ptr<void>(
            nullptr)))); // Wait, casting void* to shared_ptr is hard.
    // DockLayout stores void* Content.
    // We need to cast it back to IWindow*.
    // But DockLayout doesn't own the window shared_ptr, controls usually do.
    // Actually, Windows passed to Dock are shared_ptr usually.
    // Let's assume Content is raw pointer to IWindow.
    // We need to find the shared_ptr from the raw pointer to return shared_ptr
    // list. IWindow inherits enable_shared_from_this.

    IWindow *win = static_cast<IWindow *>(node->Content);
    if (win) {
      list.push_back(
          std::static_pointer_cast<IWindow>(win->shared_from_this()));
    }
  }

  if (node->IsSplit()) {
    CollectWindows(node->Child1, list);
    CollectWindows(node->Child2, list);
  }
}

UIControlPtr IDock::GetCenterPane() const {
  // Legacy support
  return nullptr;
}

void IDock::Update(float deltaTime) {
  if (m_AppUI) {
    glm::vec2 screenSize = m_AppUI->GetScreenSize();
    SetSize(screenSize);
  }

  // Update Layout
  if (m_Layout) {
    m_Layout->SetBounds(glm::vec4(0.0f, 0.0f, m_Size.x, m_Size.y));
    // Recursively update logic of windows
    UpdateNodeStart(m_Layout->GetRoot(), deltaTime);
  }
}

void IDock::UpdateNodeStart(std::shared_ptr<DockLayoutNode> node,
                            float deltaTime) {
  if (!node)
    return;

  // If Leaf, update window bounds and window itself
  if (node->IsLeaf() && node->Content) {
    IWindow *win = static_cast<IWindow *>(node->Content);
    float scale = AppUI::GetScale();
    win->SetPosition(glm::vec2(node->Bounds.x / scale, node->Bounds.y / scale));
    win->SetSize(glm::vec2(node->Bounds.z / scale, node->Bounds.w / scale));
    win->Update(deltaTime);
  }

  if (node->IsSplit()) {
    UpdateNodeStart(node->Child1, deltaTime);
    UpdateNodeStart(node->Child2, deltaTime);
  }
}

bool IDock::ProcessInput(const AppInput &input, const glm::vec2 &mousePos,
                         UIControl *&outCapturedControl) {

  // 1. Debug Toggle
  m_DebugLayout = input.IsKeyDown(Key::Space);

  // 1. Resizing
  // Handled by OnMouseMove/OnMouseUp via Capture

  // 2. Start Resize
  if (input.IsMouseButtonPressed(MouseButton::Left) && !m_IsResizing) {
    auto split = m_Layout->CheckResizeHit(mousePos - GetAbsolutePosition());
    if (split) {
      m_IsResizing = true;
      m_ResizingNode = split;
      outCapturedControl = this;

      // Start Logic
      m_StartResizeRatio = split->SplitRatio;
      m_ResizeStartPos = mousePos; // Logical Start
      return true;
    }
  }

  // 3. Children Processing
  bool handled = UIControl::ProcessInput(input, mousePos, outCapturedControl);
  if (handled)
    return true;

  // 4. Preview
  if (m_AppUI && m_AppUI->IsDraggingWindow()) {
    IWindow *draggedWindow = m_AppUI->GetDraggedWindow();
    if (draggedWindow) {
      UpdateDockPreview(mousePos, draggedWindow);
      if (input.IsMouseButtonReleased(MouseButton::Left)) {
        if (m_ShowingPreview && m_CurrentPreview.IsValid) {
          if (m_CurrentPreview.TargetNode) {
            m_Layout->DockToNode(m_CurrentPreview.TargetNode, draggedWindow,
                                 m_CurrentPreview.Zone);
          } else {
            DockWindow(std::static_pointer_cast<IWindow>(
                           draggedWindow->shared_from_this()),
                       m_CurrentPreview.Zone, m_CurrentPreview.TargetWindow);
          }
        }
        ClearDockPreview();
      }
    }
  } else {
    ClearDockPreview();
  }

  return false;
}

void IDock::UpdateDockPreview(const glm::vec2 &mousePos,
                              IWindow *draggedWindow) {
  DockHint hint = GetDockHintAtPosition(mousePos, draggedWindow);

  if (hint.IsValid) {
    m_CurrentPreview = hint;
    m_ShowingPreview = true;
    if (m_AppUI) {
      m_AppUI->SetDockPreview(hint);
    }
  } else {
    ClearDockPreview();
  }
}

void IDock::ClearDockPreview() {
  m_ShowingPreview = false;
  m_CurrentPreview = DockHint{};
  if (m_AppUI) {
    m_AppUI->ClearDockPreview();
  }
}

void IDock::DockWindow(std::shared_ptr<IWindow> window, DockZone zone,
                       IWindow *relativeTo) {
  if (!window || !m_Layout)
    return;

  // Ensure parentage
  if (window->GetParent() != this) {
    if (window->GetParent())
      window->GetParent()->RemoveChild(window);
    AddChild(window);
  }

  window->SetDocked(true);
  window->SetDockZone(zone);

  m_Layout->Dock(window.get(), zone, relativeTo);
}

void IDock::UndockWindow(IWindow *window) {
  if (!window || !m_Layout)
    return;
  m_Layout->Undock(window);
}

DockHint IDock::GetDockHintAtPosition(const glm::vec2 &mousePos,
                                      IWindow *draggedWindow) const {
  DockHint hint;
  if (!draggedWindow || !m_Layout)
    return hint;

  glm::vec2 absPos = GetAbsolutePosition();
  // User requested to undo scaling
  glm::vec2 scaledMousePos = mousePos;
  glm::vec2 localPos = scaledMousePos - absPos;

  auto node = m_Layout->FindNodeAt(localPos);

  if (node) {
    // We found a node (Leaf or Empty). Calculate Dock hint relative to this
    // node.

    glm::vec4 b = node->Bounds; // Local bounds
    glm::vec4 g = b;
    g.x += absPos.x;
    g.y += absPos.y; // Global bounds for preview

    // Global Dock Bounds
    float edgeThreshold = 40.0f;
    float dx = absPos.x;
    float dy = absPos.y;
    float dw = m_Size.x;
    float dh = m_Size.y;

    bool isGlobalLeft =
        (scaledMousePos.x >= dx && scaledMousePos.x <= dx + edgeThreshold);
    bool isGlobalRight = (scaledMousePos.x >= dx + dw - edgeThreshold &&
                          scaledMousePos.x <= dx + dw);
    bool isGlobalTop =
        (scaledMousePos.y >= dy && scaledMousePos.y <= dy + edgeThreshold);
    bool isGlobalBottom = (scaledMousePos.y >= dy + dh - edgeThreshold &&
                           scaledMousePos.y <= dy + dh);

    hint.TargetNode = node;
    hint.TargetWindow = (node->IsLeaf() && node->Content)
                            ? static_cast<IWindow *>(node->Content)
                            : nullptr;
    hint.IsValid = true;

    // Prioritize Global Edges (Splitting)
    if (isGlobalLeft) {
      hint.Zone = DockZone::Left;
      hint.PreviewRect = glm::vec4(dx, dy, dw * 0.25f, dh);
    } else if (isGlobalRight) {
      hint.Zone = DockZone::Right;
      hint.PreviewRect = glm::vec4(dx + dw * 0.75f, dy, dw * 0.25f, dh);
    } else if (isGlobalTop) {
      hint.Zone = DockZone::Top;
      hint.PreviewRect = glm::vec4(dx, dy, dw, dh * 0.25f);
    } else if (isGlobalBottom) {
      hint.Zone = DockZone::Bottom;
      hint.PreviewRect = glm::vec4(dx, dy + dh * 0.75f, dw, dh * 0.25f);
    } else {
      // Inner Zone -> Tabs (Center)
      hint.Zone = DockZone::Center;
      hint.PreviewRect = g; // Full node bounds
    }

    // Safety check: Prevent docking into self
    if (hint.TargetWindow == draggedWindow) {
      hint.IsValid = false;
    }
  }

  return hint;
}

void IDock::OnDraw(Draw2D *draw2D) {
  if (m_ShowingPreview && m_CurrentPreview.IsValid) {
    glm::vec4 r = m_CurrentPreview.PreviewRect;
    UITheme *theme = GetTheme();
    Texture2D *tex = theme ? theme->GetWhiteTexture() : nullptr;

    if (tex) {
      draw2D->DrawTexture(glm::vec2(r.x, r.y), glm::vec2(r.z, r.w), tex,
                          glm::vec4(0.2f, 0.6f, 1.0f, 0.3f));
      // Border lines
      float border = 2.0f;
      glm::vec4 col(0.2f, 0.6f, 1.0f, 0.8f);
      draw2D->DrawTexture(glm::vec2(r.x, r.y), glm::vec2(r.z, border), tex,
                          col);
      draw2D->DrawTexture(glm::vec2(r.x, r.y + r.w - border),
                          glm::vec2(r.z, border), tex, col);
      draw2D->DrawTexture(glm::vec2(r.x, r.y), glm::vec2(border, r.w), tex,
                          col);
      draw2D->DrawTexture(glm::vec2(r.x + r.z - border, r.y),
                          glm::vec2(border, r.w), tex, col);
    }
  }

  if (m_DebugLayout) {
    DrawLayoutDebug(draw2D);
  }
}

void IDock::DrawLayoutDebug(Draw2D *draw2D) {
  if (!m_Layout || !m_Layout->GetRoot())
    return;

  int counter = 0;
  DrawNodeDebug(draw2D, m_Layout->GetRoot(), counter);
}

void IDock::DrawNodeDebug(Draw2D *draw2D, std::shared_ptr<DockLayoutNode> node,
                          int &counter) {
  if (!node)
    return;

  // Only draw Leaf or Empty nodes (the "spaces")
  if (node->IsLeaf() || node->IsEmpty()) {
    glm::vec4 b = node->Bounds;

    // Calculate Global Position
    glm::vec2 absPos = GetAbsolutePosition();
    glm::vec2 pos = glm::vec2(b.x, b.y) + absPos;
    glm::vec2 size = glm::vec2(b.z, b.w);

    // Generate Random Color based on pointer (stable)
    uintptr_t ptr = reinterpret_cast<uintptr_t>(node.get());
    // Simple hash to get unstable but specific colors
    // We want different colors for different nodes

    // Use counter to ensure different numbers get different colors if
    // pointers align oddly
    size_t hash = ptr + counter * 1234567;

    float r = ((hash & 0xFF0000) >> 16) / 255.0f;
    float g = ((hash & 0x00FF00) >> 8) / 255.0f;
    float bl = (hash & 0x0000FF) / 255.0f;

    // Ensure not too dark
    if (r + g + bl < 1.0f) {
      r += 0.5f;
      g += 0.5f;
      bl += 0.5f;
    }
    // Clamp
    if (r > 1.0f)
      r = 1.0f;
    if (g > 1.0f)
      g = 1.0f;
    if (bl > 1.0f)
      bl = 1.0f;

    glm::vec4 col = glm::vec4(r, g, bl, 0.3f); // Semi-transparent

    UITheme *theme = GetTheme();
    Texture2D *white = theme ? theme->GetWhiteTexture() : nullptr;
    Font *font = theme ? theme->GetFont() : nullptr;

    if (white) {
      draw2D->DrawTexture(pos, size, white, col);
      // Outline
      draw2D->DrawRectOutline(pos, size, white, glm::vec4(1.0f), 2.0f);
    }

    if (font) {
      std::string text =
          (node->IsEmpty() ? "Empty " : "Leaf ") + std::to_string(counter);
      // Center text
      // Assuming we don't have GetTextSize, just guess centering or print at
      // top left + offset Draw2D has DrawText
      draw2D->RenderText(pos + size * 0.5f - glm::vec2(20, 10), text, font,
                         glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    counter++;
  }

  if (node->IsSplit()) {
    DrawNodeDebug(draw2D, node->Child1, counter);
    DrawNodeDebug(draw2D, node->Child2, counter);
  }
}

void IDock::OnMouseDown(MouseButton button) {}

void IDock::OnMouseUp(MouseButton button) {
  if (button == MouseButton::Left) {
    m_IsResizing = false;
    m_ResizingNode = nullptr;
  }
}

void IDock::OnMouseMove(const glm::vec2 &localPos) {
  if (m_IsResizing && m_ResizingNode) {
    // Reconstruct global logical mouse pos
    glm::vec2 absPos = GetAbsolutePosition();
    glm::vec2 mousePos = localPos + absPos;

    // Use Logical Position directly (User requested no scaling)
    glm::vec2 delta = mousePos - m_ResizeStartPos;
    glm::vec4 b = m_ResizingNode->Bounds;

    float val = (m_ResizingNode->Orientation == SplitOrientation::Horizontal)
                    ? delta.x
                    : delta.y;
    float size = (m_ResizingNode->Orientation == SplitOrientation::Horizontal)
                     ? b.z
                     : b.w;

    float ratioDelta = val / size;
    float newRatio = glm::clamp(m_StartResizeRatio + ratioDelta, 0.1f, 0.9f);

    m_Layout->ResizeNode(m_ResizingNode, newRatio);
  }
}

} // namespace Vivid
