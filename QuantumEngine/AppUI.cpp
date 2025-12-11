#include "AppUI.h"
#include "IDock.h"
#include "IWindow.h"
#include <cstdio>

namespace Vivid {

float AppUI::s_GlobalScale = 1.0f;

AppUI::AppUI()
    : m_Root(std::make_shared<UIControl>()), m_CapturedControl(nullptr),
      m_Draw2DPtr(nullptr), m_CurrentTheme(new ThemeDarkUI()),
      m_ScreenSize(800.0f, 600.0f), m_LastMousePos(0.0f) {
  // Root is invisible and covers the whole screen
  m_Root->SetPosition(glm::vec2(0.0f));
  m_Root->SetSize(m_ScreenSize);
  m_Root->SetVisible(true);
  m_Root->SetColor(glm::vec4(0.0f)); // Transparent
  m_Root->SetText("ROOT");
}

AppUI::~AppUI() {
  Clear();
  delete m_CurrentTheme;
}

void AppUI::Init(Draw2D *draw2D, VividDevice *device) {
  m_Draw2DPtr = draw2D;

  if (m_CurrentTheme) {
    m_CurrentTheme->Init(device);
  }
}

void AppUI::SetTheme(UITheme *theme) {
  if (theme != m_CurrentTheme) {
    delete m_CurrentTheme;
    m_CurrentTheme = theme;
  }
}

void AppUI::AddControl(UIControlPtr control) {
  if (m_Root && control) {
    control->SetTheme(m_CurrentTheme);
    m_Root->AddChild(control);
  }
}

void AppUI::RemoveControl(UIControlPtr control) {
  if (m_Root && control) {
    m_Root->RemoveChild(control);
  }
}

void AppUI::Clear() {
  if (m_Root) {
    m_Root->ClearChildren();
  }
}

UIControl *AppUI::GetControlAt(const glm::vec2 &mousePos) {
  if (m_Root) {
    return m_Root->GetControlAt(mousePos);
  }
  return nullptr;
}

void AppUI::ProcessInput(const AppInput &input) {
  if (!m_Root)
    return;

  glm::vec2 mousePos = input.GetMousePosition();

  // Handle Capture
  if (m_CapturedControl) {
    // If Captured, route input directly to it (bypassing hierarchy)
    // Transform mousePos to local space of the captured control?
    // UIControl::OnMouseMove expects Local Position if called via ProcessInput
    // logic? Wait, OnMouseMove expects (mousePos - AbsPos).

    glm::vec2 absPos = m_CapturedControl->GetAbsolutePosition();
    glm::vec2 localPos = mousePos - absPos;

    // Force Mouse Move
    m_CapturedControl->OnMouseMove(localPos);

    // Handle Mouse Up (Release Capture)
    // Check Left/Right/Middle
    if (input.IsMouseButtonReleased(MouseButton::Left)) {
      m_CapturedControl->OnMouseUp(MouseButton::Left);
      // If this was the button that started capture...
      // We assume Left for simplicity, or we should track which button.
      // For now, release on Left Up.
      m_CapturedControl = nullptr;
    }
    if (m_CapturedControl && input.IsMouseButtonReleased(MouseButton::Right)) {
      m_CapturedControl->OnMouseUp(MouseButton::Right);
    }

    // Also handle Key inputs if focused?
    // Focus is separate from Capture.

    m_LastMousePos = mousePos;
    return;
  }

  // First, process passive inputs (like mouse wheel) - these work regardless
  // of focus/capture state
  m_Root->ProcessPassiveInput(input, mousePos);

  // Normal Processing
  UIControl *newCapture = nullptr;
  m_Root->ProcessInput(input, mousePos, newCapture);

  if (newCapture) {
    // Capture started
    m_CapturedControl = newCapture->shared_from_this();
    printf("Capture Started: %s\n", m_CapturedControl->GetText().c_str());

    // Bring Root-Level Ancestor to Front (but NOT for docked windows)
    UIControl *ancestor = newCapture;
    while (ancestor && ancestor->GetParent() != m_Root.get()) {
      ancestor = ancestor->GetParent();
    }

    if (ancestor && ancestor->GetParent() == m_Root.get()) {
      // Skip z-order change for IDock (the docking container should stay at
      // back)
      IDock *dock = dynamic_cast<IDock *>(ancestor);
      if (!dock) {
        m_Root->MoveChildToFront(ancestor);
      }
    }
  }

  m_LastMousePos = mousePos;
}

void AppUI::Update(float deltaTime) {
  if (m_Root) {
    m_Root->Update(deltaTime);
  }
}

void AppUI::Render() {
  if (!m_Root || !m_Draw2DPtr)
    return;

  m_Root->Draw(m_Draw2DPtr);
}

void AppUI::SetScreenSize(float width, float height) {
  m_ScreenSize = glm::vec2(width, height);
  if (m_Root) {
    // Root size should be Logical, so that GetSize() returns Pixels matching
    // ScreenSize
    m_Root->SetSize(m_ScreenSize / s_GlobalScale);
  }
  if (m_Dock) {
    m_Dock->SetSize(m_ScreenSize / s_GlobalScale);
    // Force layout update?
    // IDock::Update will handle it next frame, but if we dock immediately after
    // resize? Let's assume Update is sufficient, but setting size is good.
  }
}

// --- Docking Infrastructure ---

void AppUI::StartDragOperation(IWindow *window, const glm::vec2 &startPos,
                               const glm::vec2 &offset) {
  m_DragOp.IsActive = true;
  m_DragOp.DraggedWindow = window;
  m_DragOp.DragSource = window;
  m_DragOp.DragStartPos = startPos;
  m_DragOp.DragOffset = offset;
}

void AppUI::EndDragOperation() {
  m_DragOp.IsActive = false;
  m_DragOp.DraggedWindow = nullptr;
  m_DragOp.DragSource = nullptr;
  m_DragOp.DragStartPos = glm::vec2(0.0f);
  m_DragOp.DragOffset = glm::vec2(0.0f);
  ClearDockPreview();
}

void AppUI::SetDockPreview(const DockHint &hint) { m_DockPreview = hint; }

void AppUI::ClearDockPreview() {
  m_DockPreview = DockHint{}; // Reset to default
}

DockHint AppUI::GetDockHintAt(const glm::vec2 &mousePos,
                              IWindow *draggedWindow) {
  // Use IDock if available
  if (m_Dock) {
    return m_Dock->GetDockHintAtPosition(mousePos, draggedWindow);
  }
  return DockHint{};
}

void AppUI::SetDock(std::shared_ptr<IDock> dock) {
  // Remove existing dock if any
  if (m_Dock) {
    RemoveControl(m_Dock);
  }

  m_Dock = dock;

  if (m_Dock) {
    m_Dock->SetAppUI(this);
    m_Dock->SetTheme(m_CurrentTheme);

    // Add as first child (behind all other controls)
    if (m_Root) {
      m_Root->AddChild(m_Dock);
      m_Dock->SendToBack(); // Ensure dock is at the back (renders first)
    }
  }
}

std::shared_ptr<IDock> AppUI::CreateDock() {
  auto dock = std::make_shared<IDock>();
  SetDock(dock);
  return dock;
}

} // namespace Vivid
