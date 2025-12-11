#include "IWindow.h"
#include "AppUI.h"
#include "Draw2D.h"
#include "IDock.h"
#include "IHorizontalScroller.h"
#include "IVerticalScroller.h"
#include "UITheme.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Vivid {

IWindow::IWindow(const std::string &title)
    : m_ActiveTabIndex(0), m_TabHeaderHeight(20.0f), m_IsDragging(false),
      m_IsResizing(false), m_IsDocked(false), m_IsTab(false),
      m_TitleBarHeight(20.0f), m_DragStartPos(0.0f), m_ResizeStartSize(0.0f),
      m_IsDraggingTab(false), m_IsProxyDragging(false),
      m_ProxyDragWindow(nullptr), m_TabDragOffsetX(0.0f),
      m_PotentialDockTarget(nullptr), m_DragTearsTab(false),
      // Initialize Content Root (clipping managed in Draw() using fixed client
      // area)
      m_VScroller(nullptr), m_HScroller(nullptr), m_ScrollOffset(0.0f),
      m_DockZone(DockZone::None) {
  SetText(title);
  SetSize(glm::vec2(400, 300));
  SetMinSize(glm::vec2(256.0f, 128.0f));

  // Initialize Content Root (clipping managed in Draw() using fixed client
  // area)
  m_ContentRoot = std::make_shared<UIControl>();
  m_ContentRoot->SetSize(GetSize());
  // Add as direct child (bypassing our override)
  UIControl::AddChild(m_ContentRoot);
}

const std::vector<UIControlPtr> &IWindow::GetContentControls() const {
  if (m_ContentRoot) {
    return m_ContentRoot->GetChildren();
  }
  static std::vector<UIControlPtr> empty;
  return empty;
}

void IWindow::AddChild(UIControlPtr child) {
  if (!child)
    return;

  // Safety check: Don't add m_ContentRoot to itself
  if (child == m_ContentRoot)
    return;

  std::shared_ptr<IWindow> asWindow = std::dynamic_pointer_cast<IWindow>(child);
  if (asWindow) {
    UIControl::AddChild(child);
  } else if (m_ContentRoot) {
    m_ContentRoot->AddChild(child);
  } else {
    // Fallback if ContentRoot missing (shouldn't happen)
    UIControl::AddChild(child);
  }
}

void IWindow::RemoveChild(UIControlPtr child) {
  if (m_ContentRoot) {
    // Try removing from content root first
    m_ContentRoot->RemoveChild(child);
  }
  // Also try direct (for tabs)
  UIControl::RemoveChild(child);
}

bool IWindow::ProcessInput(const AppInput &input, const glm::vec2 &mousePos,
                           UIControl *&outCapturedControl) {
  if (!IsVisible() || !IsEnabled())
    return false;

  // For docked windows, use the window bounds as the input clip region
  // For non-docked windows, we'll handle resize and use client area later

  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 size = GetSize();
  float scale = AppUI::GetScale();

  // Check resize zone FIRST (only for non-docked windows)
  if (!m_IsDocked) {
    float resizeZone = 10.0f * scale;

    bool inResizeZone = (mousePos.x >= absPos.x + size.x - resizeZone &&
                         mousePos.x <= absPos.x + size.x &&
                         mousePos.y >= absPos.y + size.y - resizeZone &&
                         mousePos.y <= absPos.y + size.y);

    if (inResizeZone && input.IsMouseButtonPressed(MouseButton::Left)) {
      // Start resize - this takes priority over children
      m_IsResizing = true;
      m_DragStartPos = mousePos;
      m_ResizeStartSize = GetSize();
      outCapturedControl = this;
      return true;
    }
  }

  // Determine clip region for m_ContentRoot input
  // Both docked and non-docked windows: content is below the title bar
  bool mouseInContentRegion;
  glm::vec4 clientArea = GetClientArea();
  mouseInContentRegion =
      (mousePos.x >= clientArea.x &&
       mousePos.x <= clientArea.x + clientArea.z &&
       mousePos.y >= clientArea.y && mousePos.y <= clientArea.y + clientArea.w);

  // Process children manually - skip m_ContentRoot if mouse outside client area
  const auto &children = UIControl::GetChildren();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    const auto &child = *it;

    // Skip m_ContentRoot if mouse is outside visible content region
    // Also clear hover states on content children to trigger OnMouseLeave
    if (child.get() == m_ContentRoot.get() && !mouseInContentRegion) {
      m_ContentRoot->ClearHoverState();
      continue;
    }

    if (child->ProcessInput(input, mousePos, outCapturedControl)) {
      return true;
    }
  }

  // Check if mouse is over this window
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
  if (isOver) {
    if (input.IsMouseButtonPressed(MouseButton::Left)) {
      OnMouseDown(MouseButton::Left);
      m_WasMouseDown = true;
      m_Focused = true;
      OnFocusGained();
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
    }
    if (input.IsMouseButtonReleased(MouseButton::Right)) {
      OnMouseUp(MouseButton::Right);
    }
    if (input.IsMouseButtonReleased(MouseButton::Middle)) {
      OnMouseUp(MouseButton::Middle);
    }

    return true;
  } else {
    if (input.IsMouseButtonPressed(MouseButton::Left) && m_Focused) {
      m_Focused = false;
      OnFocusLost();
    }
    m_WasMouseDown = false;
  }

  return false;
}

bool IWindow::ProcessPassiveInput(const AppInput &input,
                                  const glm::vec2 &mousePos) {
  if (!IsVisible() || !IsEnabled())
    return false;

  // First let children handle passive input
  const auto &children = UIControl::GetChildren();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    if ((*it)->ProcessPassiveInput(input, mousePos)) {
      return true;
    }
  }

  // Check if mouse is over this window's content area
  bool mouseInContentRegion;
  if (m_IsDocked) {
    mouseInContentRegion = Contains(mousePos);
  } else {
    glm::vec4 clientArea = GetClientArea();
    mouseInContentRegion = (mousePos.x >= clientArea.x &&
                            mousePos.x <= clientArea.x + clientArea.z &&
                            mousePos.y >= clientArea.y &&
                            mousePos.y <= clientArea.y + clientArea.w);
  }

  if (mouseInContentRegion) {
    // Handle mouse wheel scrolling
    glm::vec2 scrollDelta = input.GetScrollDelta();
    if (m_VScroller && m_VScroller->IsVisible() &&
        std::abs(scrollDelta.y) > 0.001f) {
      // Calculate scroll step based on content/view ratio
      float scrollStep = 0.1f; // Default step of 10% of the scroll range
      if (m_VScroller->GetContentSize() > m_VScroller->GetViewSize()) {
        // Smaller steps for larger content (more precise scrolling)
        float ratio =
            m_VScroller->GetViewSize() / m_VScroller->GetContentSize();
        scrollStep = ratio * 0.5f; // Half a view height per scroll notch
        scrollStep = glm::clamp(scrollStep, 0.02f, 0.2f);
      }
      float newValue = m_VScroller->GetValue() - scrollDelta.y * scrollStep;
      m_VScroller->SetValue(newValue);
    }
    // Consume the input if mouse is over this window - prevents underlying
    // windows from receiving scroll events (respects z-order)
    return true;
  }

  return false;
}

void IWindow::StartDrag(const glm::vec2 &globalMouse) {
  m_IsDragging = true;
  m_WindowStartPos = GetPosition();
  m_DragStartPos = globalMouse;

  // Notify AppUI that a drag operation started (for docking preview)
  UIControl *root = this;
  while (root->GetParent()) {
    root = root->GetParent();
  }
  // Try to find AppUI through the dock
  // We need to get access to AppUI - the dock holds a reference
  // For now, we'll handle via static access if possible
}

void IWindow::DetachTab(int tabIndex) {
  if (tabIndex < 0 || tabIndex >= m_Tabs.size())
    return;

  IWindow *tabRaw = m_Tabs[tabIndex];

  // Tab is a direct child of 'this', NOT m_ContentRoot.
  // So we search in 'UIControl::GetChildren()', i.e. our direct children.
  auto &children =
      const_cast<std::vector<UIControlPtr> &>(UIControl::GetChildren());
  auto it = std::find_if(
      children.begin(), children.end(),
      [tabRaw](const UIControlPtr &p) { return p.get() == tabRaw; });

  if (it != children.end()) {
    UIControlPtr tabPtr = *it; // Keep alive

    // Remove from here (Direct remove)
    UIControl::RemoveChild(tabPtr);
    m_Tabs.erase(m_Tabs.begin() + tabIndex);

    if (m_ActiveTabIndex >= m_Tabs.size())
      m_ActiveTabIndex = m_Tabs.empty() ? 0 : (int)m_Tabs.size() - 1;

    // Find Root
    UIControl *root = this;
    while (root->GetParent()) {
      root = root->GetParent();
    }

    // Add to Root
    root->AddChild(tabPtr);

    // Setup Window State
    tabRaw->SetDocked(false);
    tabRaw->m_IsTab = false; // No longer a tab
    tabRaw->SetVisible(true);
    tabRaw->SetTheme(GetTheme()); // Propagate theme
  }
}

void IWindow::AddTab(std::shared_ptr<IWindow> window, bool makeActive) {
  if (m_Tabs.empty()) {
    // Convert current content into Tab 1
    auto tab1 = std::make_shared<IWindow>(GetText());
    tab1->SetTheme(GetTheme());

    // Move children from m_ContentRoot to tab1
    // tab1 has its own m_ContentRoot.
    if (m_ContentRoot) {
      auto contentChildren = m_ContentRoot->GetChildren(); // Copy vector
      m_ContentRoot->ClearChildren();

      for (auto &child : contentChildren) {
        tab1->AddChild(child); // Adds to tab1->m_ContentRoot
      }
    }

    tab1->SetDocked(true);
    tab1->m_IsTab = true; // Mark as tab so it doesn't draw its own title bar
    // Add tab1 as direct child of 'this'
    UIControl::AddChild(tab1);
    m_Tabs.push_back(tab1.get());
  }

  // Add new window
  UIControl::AddChild(window);
  m_Tabs.push_back(window.get());
  window->SetDocked(true);
  window->m_IsTab = true; // Mark as tab so it doesn't draw its own title bar
  window->SetTheme(GetTheme()); // Theme for new tab

  // Make the newly added tab active only if requested (e.g. from drag/drop)
  if (makeActive) {
    m_ActiveTabIndex = (int)m_Tabs.size() - 1;
  }
}

void IWindow::Update(float deltaTime) {
  UIControl::Update(deltaTime);

  // Header offset applies to windows that draw their own title bar
  // Tab windows (m_IsTab) don't draw their own title, so no offset needed
  float headerOffset = 0.0f;
  if (!m_IsTab) {
    headerOffset = (!m_Tabs.empty() ? m_TabHeaderHeight : m_TitleBarHeight);
  }
  float scrollerWidth = 10.0f; // Logical

  // Calculate content bounding box (logical units)
  glm::vec2 contentSize(0.0f);
  if (m_ContentRoot) {
    for (auto &child : m_ContentRoot->GetChildren()) {
      if (!child)
        continue;
      glm::vec2 childEnd =
          child->GetPosition() +
          (child->GetSize() / AppUI::GetScale()); // Convert to logical
      if (childEnd.x > contentSize.x)
        contentSize.x = childEnd.x;
      if (childEnd.y > contentSize.y)
        contentSize.y = childEnd.y;
    }
  }

  // Client area size (logical)
  glm::vec2 clientSize = m_Size - glm::vec2(0.0f, headerOffset);

  // Determine if scrollers are needed
  bool needVScroll = contentSize.y > clientSize.y;
  bool needHScroll = contentSize.x > clientSize.x;

  // Adjust client size if scrollers are present
  if (needVScroll)
    clientSize.x -= scrollerWidth;
  if (needHScroll)
    clientSize.y -= scrollerWidth;

  // Create scrollers if needed
  if (needVScroll && !m_VScroller) {
    auto vScroller = std::make_shared<IVerticalScroller>();
    vScroller->SetTheme(GetTheme());
    m_VScroller = vScroller.get();
    UIControl::AddChild(std::static_pointer_cast<UIControl>(vScroller));
  }
  if (needHScroll && !m_HScroller) {
    auto hScroller = std::make_shared<IHorizontalScroller>();
    hScroller->SetTheme(GetTheme());
    m_HScroller = hScroller.get();
    UIControl::AddChild(std::static_pointer_cast<UIControl>(hScroller));
  }

  // Update scroller visibility and properties
  // Resize is handled via ProcessInput priority, so scrollers can be
  // full-size
  if (m_VScroller) {
    m_VScroller->SetVisible(needVScroll);
    if (needVScroll) {
      m_VScroller->SetContentSize(contentSize.y);
      m_VScroller->SetViewSize(clientSize.y);
      m_VScroller->SetPosition(
          glm::vec2(m_Size.x - scrollerWidth, headerOffset));
      m_VScroller->SetSize(glm::vec2(scrollerWidth, clientSize.y));
    }
  }
  if (m_HScroller) {
    m_HScroller->SetVisible(needHScroll);
    if (needHScroll) {
      m_HScroller->SetContentSize(contentSize.x);
      m_HScroller->SetViewSize(clientSize.x);
      m_HScroller->SetPosition(glm::vec2(0.0f, m_Size.y - scrollerWidth));
      m_HScroller->SetSize(glm::vec2(clientSize.x, scrollerWidth));
    }
  }

  // Calculate scroll offset
  m_ScrollOffset = glm::vec2(0.0f);
  if (m_HScroller && needHScroll) {
    float maxScrollX = contentSize.x - clientSize.x;
    m_ScrollOffset.x = m_HScroller->GetValue() * maxScrollX;
  }
  if (m_VScroller && needVScroll) {
    float maxScrollY = contentSize.y - clientSize.y;
    m_ScrollOffset.y = m_VScroller->GetValue() * maxScrollY;
  }

  // Update Content Root Position (LOGICAL)
  if (m_ContentRoot) {
    glm::vec2 basePos(0.0f, headerOffset);
    m_ContentRoot->SetPosition(basePos - m_ScrollOffset);
    m_ContentRoot->SetSize(clientSize);
  }

  if (!m_Tabs.empty()) {
    float scale = AppUI::GetScale();
    float headerH = m_TabHeaderHeight; // Logical

    for (size_t i = 0; i < m_Tabs.size(); ++i) {
      IWindow *tab = m_Tabs[i];
      if (!tab)
        continue;

      bool isActive = (i == m_ActiveTabIndex);
      tab->SetVisible(isActive);
      if (isActive) {
        tab->SetDocked(true);
        tab->SetPosition(glm::vec2(0.0f, headerH));
        tab->SetSize(glm::vec2(m_Size.x, m_Size.y - headerH));
      }
    }
  }
}

glm::vec4 IWindow::GetTitleBarRect() const {
  glm::vec2 pos = GetAbsolutePosition();
  glm::vec2 size = GetSize();
  float scale = AppUI::GetScale();
  // Returns Global Rect (Pixels)
  float h = (!m_Tabs.empty() ? m_TabHeaderHeight : m_TitleBarHeight) * scale;
  return glm::vec4(pos.x, pos.y, size.x, h);
}

glm::vec4 IWindow::GetClientArea() const {
  glm::vec2 pos = GetAbsolutePosition();
  glm::vec2 size = GetSize();
  float scale = AppUI::GetScale();
  float yOffset =
      (!m_Tabs.empty() ? m_TabHeaderHeight : m_TitleBarHeight) * scale;
  return glm::vec4(pos.x, pos.y + yOffset, size.x, size.y - yOffset);
}

void IWindow::Draw(Draw2D *draw2D) {
  if (!IsVisible())
    return;

  // Draw self (window chrome)
  OnDraw(draw2D);

  // Push client area scissor before drawing children
  // This clips content to below the title bar for both docked and non-docked
  // windows
  if (draw2D) {
    glm::vec4 clipRect = GetClientArea();
    // Reduce clipping height by 3 pixels (scaled) to prevent border
    // overlap/clipping issues
    float scale = AppUI::GetScale();
    clipRect.w -= (3.0f * scale);
    draw2D->PushScissor(clipRect);
  }

  // Draw children (m_ContentRoot containing content)
  for (auto &child : UIControl::GetChildren()) {
    child->Draw(draw2D);
  }

  // Pop scissor
  if (draw2D) {
    draw2D->PopScissor();
  }
}

void IWindow::OnDraw(Draw2D *draw2D) {
  if (!IsVisible() || !draw2D)
    return;

  UITheme *theme = GetTheme();
  if (!theme)
    return;

  Texture2D *frameTex = theme->GetFrameTexture();
  Texture2D *headerTex = theme->GetHeaderTexture(); // Use gradient if available
  if (!headerTex)
    headerTex = frameTex;

  // Draw Dock Highlight (Ghost Tab)
  // Draw FIRST so the dragged window (this) renders ON TOP of the ghost tab
  if (m_IsDragging && m_PotentialDockTarget) {
    float scale = AppUI::GetScale();
    float headerH = m_TabHeaderHeight * scale;
    glm::vec2 targetPos = m_PotentialDockTarget->GetAbsolutePosition();
    float currentTabsWidth = 0.0f;

    Font *font = theme->GetFont();

    if (m_PotentialDockTarget->m_Tabs.empty()) {
      // Target acts as "Tab 0"
      float w = 100.0f * scale;
      if (font) {
        glm::vec2 ts =
            font->MeasureText(m_PotentialDockTarget->GetText()) * scale;
        w = ts.x + (20.0f * scale);
        if (w < 60.0f * scale)
          w = 60.0f * scale;
      }
      currentTabsWidth += w;
    } else {
      for (auto *tab : m_PotentialDockTarget->m_Tabs) {
        float w = 100.0f * scale;
        if (font) {
          glm::vec2 ts = font->MeasureText(tab->GetText()) * scale;
          w = ts.x + (20.0f * scale);
          if (w < 60.0f * scale)
            w = 60.0f * scale;
        }
        currentTabsWidth += w;
      }
    }

    // Ghost Tab Position
    glm::vec2 ghostPos = glm::vec2(targetPos.x + currentTabsWidth, targetPos.y);

    // Ghost Tab Width (My Width)
    float myWidth = 100.0f * scale;
    if (font) {
      glm::vec2 ts = font->MeasureText(GetText()) * scale;
      myWidth = ts.x + (20.0f * scale);
      if (myWidth < 60.0f * scale)
        myWidth = 60.0f * scale;
    }

    // User requested: 0.2f, 0.6f, 0.6f (Teal)
    Texture2D *whiteTex = theme->GetWhiteTexture();
    if (whiteTex) {
      draw2D->DrawTexture(ghostPos, glm::vec2(myWidth, headerH), whiteTex,
                          glm::vec4(0.2f, 0.6f, 0.6f, 0.75f));
    } else if (headerTex) {
      draw2D->DrawTexture(ghostPos, glm::vec2(myWidth, headerH), headerTex,
                          glm::vec4(0.2f, 0.6f, 0.6f, 0.75f));
    }

    // Draw Text (Ghost)
    if (font) {
      glm::vec2 ts = font->MeasureText(GetText()) * scale;
      float xOffset = (myWidth - ts.x) * 0.5f;
      float yOffset = (headerH - ts.y) * 0.5f;
      draw2D->DrawText(glm::vec2(ghostPos.x + xOffset,
                                 ghostPos.y + yOffset - (1.0f * scale)),
                       GetText(), font, glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    }
  }

  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 size = GetSize();

  // Draw Background
  if (frameTex) {
    draw2D->DrawTexture(absPos, size, frameTex,
                        theme->GetWindowBackgroundColor());
  }

  // Draw Thin White Outline
  Texture2D *whiteTex = theme->GetWhiteTexture();
  if (whiteTex) {
    draw2D->DrawRectOutline(absPos, size, whiteTex,
                            glm::vec4(1.0f, 1.0f, 1.0f, 0.4f), 1.0f);
  }

  // Draw header/tabs - but skip if this window is a tab inside another window
  // (the parent window draws the tab bar, so tabs shouldn't draw their own
  // title)
  if (!m_IsTab) {
    if (m_Tabs.empty()) {
      // No tabs: draw normal title bar
      glm::vec4 titleRect = GetTitleBarRect();
      if (headerTex)
        draw2D->DrawTexture(glm::vec2(titleRect.x, titleRect.y),
                            glm::vec2(titleRect.z, titleRect.w), headerTex,
                            glm::vec4(1.0f));

      Font *font = theme->GetFont();
      if (font) {
        std::string title = GetText();
        float scaleVal = AppUI::GetScale();
        glm::vec2 textSize = font->MeasureText(title) * scaleVal;
        float yCenter =
            titleRect.y + (titleRect.w - textSize.y) * 0.5f - (3.0f * scaleVal);
        draw2D->DrawText(glm::vec2(titleRect.x + 10.0f, yCenter), title, font,
                         theme->GetTitleTextColor());
      }
    } else {
      // Has tabs: draw tab bar
      DrawTabs(draw2D);
    }
  }
}

void IWindow::DrawTabs(Draw2D *draw2D) {
  UITheme *theme = GetTheme();
  glm::vec2 absPos = GetAbsolutePosition();
  float tabY = absPos.y;
  float startX = absPos.x;

  Texture2D *headerTex = theme->GetHeaderTexture();
  if (!headerTex)
    headerTex = theme->GetFrameTexture();

  float scale = AppUI::GetScale();
  float headerH = m_TabHeaderHeight * scale; // Scaled Header

  // Draw Header Background Strip
  if (headerTex)
    draw2D->DrawTexture(
        glm::vec2(absPos.x, tabY), glm::vec2(GetSize().x, headerH), headerTex,
        glm::vec4(0.7f, 0.7f, 0.7f, 1.0f)); // Darker for inactive area

  Font *font = theme->GetFont();

  for (size_t i = 0; i < m_Tabs.size(); ++i) {
    IWindow *tab = m_Tabs[i];

    // Active tab uses full brightness gradient. Inactive uses darker.
    glm::vec4 color = (i == m_ActiveTabIndex)
                          ? glm::vec4(1.0f)
                          : glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);

    float tabWidth = 100.0f * scale; // Default scaled
    std::string t = tab->GetText();
    if (font) {
      glm::vec2 ts = font->MeasureText(t) * scale;

      tabWidth = ts.x + (20.0f * scale);
      if (tabWidth < (60.0f * scale))
        tabWidth = (60.0f * scale);
    }

    if (headerTex)
      draw2D->DrawTexture(glm::vec2(startX, tabY), glm::vec2(tabWidth, headerH),
                          headerTex, color);

    if (font) {
      glm::vec2 ts = font->MeasureText(t) * scale;

      // Center Text
      float xOffset = (tabWidth - ts.x) * 0.5f;
      float yOffset = (headerH - ts.y) * 0.5f;

      draw2D->DrawText(
          glm::vec2(startX + xOffset, tabY + yOffset - (1.0f * scale)), t, font,
          theme->GetTitleTextColor());
    }
    startX += tabWidth;
  }
}

bool IWindow::CheckTabClick(const glm::vec2 &globalMousePos) {
  if (m_Tabs.empty())
    return false;

  UITheme *theme = GetTheme();
  Font *font = theme ? theme->GetFont() : nullptr;

  glm::vec2 absPos = GetAbsolutePosition();
  float tabY = absPos.y;
  float startX = absPos.x;

  float scale = AppUI::GetScale();
  float headerH = m_TabHeaderHeight * scale;

  if (globalMousePos.y >= tabY && globalMousePos.y <= tabY + headerH) {
    for (size_t i = 0; i < m_Tabs.size(); ++i) {
      IWindow *tab = m_Tabs[i];
      float tabWidth = 100.0f * scale;
      if (font) {
        glm::vec2 ts = font->MeasureText(tab->GetText()) * scale;
        tabWidth = ts.x + (20.0f * scale);
        if (tabWidth < (60.0f * scale))
          tabWidth = (60.0f * scale);
      }

      if (globalMousePos.x >= startX && globalMousePos.x <= startX + tabWidth) {
        // Tab Clicked logic
        bool wasActive = (i == m_ActiveTabIndex);
        m_DragTearsTab = !wasActive;

        m_ActiveTabIndex = (int)i;
        m_IsDraggingTab = true;
        m_DragStartPos = globalMousePos;
        m_TabDragOffsetX = globalMousePos.x - startX;
        return true;
      }
      startX += tabWidth;
    }
  }
  return false;
}

void IWindow::OnMouseDown(MouseButton button) {
  glm::vec2 globalMouse = m_MouseStartPos + GetAbsolutePosition();

  if (button == MouseButton::Left) {
    // Check Tabs (Global Check) - works for docked and non-docked
    if (CheckTabClick(globalMouse))
      return;

    // Check Header Drag (Global Check) - for undocking when docked
    glm::vec4 headerRect = GetTitleBarRect(); // Returns Global
    if (globalMouse.x >= headerRect.x &&
        globalMouse.x <= headerRect.x + headerRect.z &&
        globalMouse.y >= headerRect.y &&
        globalMouse.y <= headerRect.y + headerRect.w) {

      // If this window is docked, undock it first before starting drag
      if (m_IsDocked) {
        // Get self as shared_ptr
        auto selfPtr = shared_from_this();
        std::shared_ptr<IWindow> selfWindow =
            std::dynamic_pointer_cast<IWindow>(selfPtr);

        if (selfWindow) {
          // Find the IDock and undock from it
          UIControl *root = this;
          while (root->GetParent()) {
            root = root->GetParent();
          }

          // Find IDock in root's children and call UndockWindow
          for (auto &child : root->GetChildren()) {
            IDock *dock = dynamic_cast<IDock *>(child.get());
            if (dock) {
              dock->UndockWindow(this);
              break;
            }
          }

          // Remove from current parent (dock pane)
          if (GetParent()) {
            GetParent()->RemoveChild(selfPtr);
          }

          // Add to root as floating window
          root->AddChild(selfPtr);

          // Mark as undocked
          m_IsDocked = false;

          // Calculate current relative offset to preserve drag point (Logical)
          glm::vec2 currentOffset = globalMouse - GetPosition();

          // Preserve current size (convert Physical GetSize to Logical)
          float scale = AppUI::GetScale();
          glm::vec2 currentSizeLogical = GetSize() / scale;
          SetSize(currentSizeLogical);

          // Since size is preserved, we can simply apply the offset
          SetPosition(globalMouse - currentOffset);
        }
      }

      m_IsDragging = true;
      m_WindowStartPos = GetPosition();
      m_DragStartPos = globalMouse;
      return;
    }

    // Skip resize for docked windows
    if (m_IsDocked)
      return;

    // Check Resize (Global Check)
    glm::vec2 absPos = GetAbsolutePosition();
    glm::vec2 size = GetSize();
    float scale = AppUI::GetScale();
    float resizeZone = 20.0f * scale; // Scale for DPI
    if (globalMouse.x >= absPos.x + size.x - resizeZone &&
        globalMouse.x <= absPos.x + size.x &&
        globalMouse.y >= absPos.y + size.y - resizeZone &&
        globalMouse.y <= absPos.y + size.y) {
      m_IsResizing = true;
      m_DragStartPos = globalMouse;
      m_ResizeStartSize = GetSize();
      return;
    }
  }
}

void IWindow::OnMouseUp(MouseButton button) {
  if (button == MouseButton::Left) {
    if (m_IsDragging) {
      // Check if there's an active dock preview - dock to IDock zone
      UIControl *root = this;
      while (root->GetParent()) {
        root = root->GetParent();
      }

      // Find IDock and check for active preview
      for (auto &child : root->GetChildren()) {
        IDock *dock = dynamic_cast<IDock *>(child.get());
        if (dock && dock->IsShowingPreview()) {
          const DockHint &preview = dock->GetCurrentPreview();
          if (preview.IsValid) {
            // Get self as shared_ptr
            auto selfPtr = shared_from_this();
            std::shared_ptr<IWindow> selfWindow =
                std::dynamic_pointer_cast<IWindow>(selfPtr);

            if (selfWindow) {
              // Remove from current parent
              UIControl *parent = GetParent();
              if (parent) {
                parent->RemoveChild(selfPtr);
              }

              // Dock to the zone
              dock->DockWindow(selfWindow, preview.Zone);
              dock->ClearDockPreview();

              m_IsDragging = false;
              m_IsResizing = false;
              m_IsDraggingTab = false;
              m_PotentialDockTarget = nullptr;
              NotifyDragEnd();
              return;
            }
          }
        }
      }

      // Tab docking to another window (existing logic)
      if (m_PotentialDockTarget) {
        auto selfPtr = shared_from_this();
        std::shared_ptr<IWindow> selfWindow =
            std::dynamic_pointer_cast<IWindow>(selfPtr);

        if (selfWindow && m_PotentialDockTarget &&
            m_PotentialDockTarget != this) {
          UIControl *parent = GetParent();
          if (parent) {
            parent->RemoveChild(selfPtr);
          }
          m_PotentialDockTarget->AddTab(selfWindow,
                                        true); // makeActive=true for drag/drop
        }
      }
    }

    m_IsDragging = false;
    m_IsResizing = false;
    m_IsDraggingTab = false;
    m_PotentialDockTarget = nullptr;
    // Notify drag ended
    NotifyDragEnd();

    if (m_IsProxyDragging && m_ProxyDragWindow) {
      m_ProxyDragWindow->OnMouseUp(button);
      m_IsProxyDragging = false;
      m_ProxyDragWindow = nullptr;
    }
  }
}

void IWindow::OnMouseMove(const glm::vec2 &localPos) {
  // Store Local
  m_MouseStartPos = localPos;
  // Construct Global
  glm::vec2 globalMouse = localPos + GetAbsolutePosition();

  // Proxy Dragging
  if (m_IsProxyDragging && m_ProxyDragWindow) {
    glm::vec2 proxyLocal =
        globalMouse - m_ProxyDragWindow->GetAbsolutePosition();
    m_ProxyDragWindow->OnMouseMove(proxyLocal);
    return;
  }

  if (m_IsDraggingTab && !m_IsProxyDragging) {
    float dist = glm::distance(globalMouse, m_DragStartPos);
    if (dist > 5.0f) {

      if (!m_DragTearsTab) {
        m_IsDragging = true;
        m_IsDraggingTab = false;
        m_WindowStartPos = GetPosition();
        // Notify root about drag for dock preview
        NotifyDragStart(globalMouse);
        return;
      }

      if (m_Tabs.size() <= 1) {
        m_IsDragging = true;
        m_IsDraggingTab = false;
        m_WindowStartPos = GetPosition();
        // Notify root about drag for dock preview
        NotifyDragStart(globalMouse);
        return;
      }

      // Trigger Detach
      IWindow *newWin = m_Tabs[m_ActiveTabIndex];
      DetachTab(m_ActiveTabIndex);

      m_ProxyDragWindow = newWin;
      m_IsProxyDragging = true;

      float scale = AppUI::GetScale();
      // Calculate New Position in Pixels
      // Offset Y by half header height (pixels)
      glm::vec2 newPosPixels = glm::vec2(globalMouse.x - m_TabDragOffsetX,
                                         globalMouse.y - (12.0f * scale));

      // Convert to Logical for SetPosition
      m_ProxyDragWindow->SetPosition(newPosPixels / scale);
      m_ProxyDragWindow->StartDrag(globalMouse);
      return;
    }
  }

  if (m_IsDragging) {
    m_PotentialDockTarget = nullptr;
    bool canDock = (m_Tabs.size() <= 1);

    if (canDock) {
      UIControl *root = this;
      while (root->GetParent()) {
        root = root->GetParent();
      }

      UIControl *hit = root->GetControlAt(globalMouse, this);

      while (hit) {
        IWindow *win = dynamic_cast<IWindow *>(hit);
        // Allow docking to any window (including docked windows) if over
        // its title bar
        if (win && win != this) {
          glm::vec4 rect = win->GetTitleBarRect();
          if (globalMouse.x >= rect.x && globalMouse.x <= rect.x + rect.z &&
              globalMouse.y >= rect.y && globalMouse.y <= rect.y + rect.w) {
            m_PotentialDockTarget = win;
            break;
          }
        }
        hit = hit->GetParent();
      }
    }

    // Delta = CurrentGlobal - StartGlobal (Pixels)
    glm::vec2 delta = globalMouse - m_DragStartPos;

    float scale = AppUI::GetScale();
    // m_WindowStartPos is LOGICAL
    // delta is PIXELS
    // We must scale delta to Logical before adding.
    SetPosition(m_WindowStartPos + (delta / scale));

    // Update dock preview continuously while dragging
    NotifyDragStart(globalMouse);

  } else if (m_IsResizing) {
    float scale = AppUI::GetScale();
    glm::vec2 delta = globalMouse - m_DragStartPos; // Pixels

    // m_ResizeStartSize comes from GetSize() which is PIXELS.
    // Convert everything to Logical
    glm::vec2 startSizeLogical = m_ResizeStartSize / scale;
    glm::vec2 newSizeLogical = startSizeLogical + (delta / scale);

    // Minimum size: 256x128 logical
    if (newSizeLogical.x < m_MinSize.x)
      newSizeLogical.x = m_MinSize.x;
    if (newSizeLogical.y < m_MinSize.y)
      newSizeLogical.y = m_MinSize.y;

    SetSize(newSizeLogical);
  }
}

// --- Docking Infrastructure Methods ---

void IWindow::Undock() {
  if (m_DockZone == DockZone::None)
    return; // Already floating

  // Mark as floating
  m_DockZone = DockZone::None;
  m_IsDocked = false;

  // Note: The actual removal from dock container will be handled by IDock
  // when it's implemented. This just updates the window's internal state.
}

void IWindow::Close() {
  if (m_OnCloseCallback) {
    m_OnCloseCallback(this);
  }
}

std::vector<DockHint> IWindow::GetDockHints(const glm::vec2 &mousePos) const {
  std::vector<DockHint> hints;

  // Only provide dock hints if not currently docked (floating windows)
  // or if we're the root of a dock layout
  if (m_IsDocked)
    return hints;

  float scale = AppUI::GetScale();
  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 size = GetSize();

  // Define dock zone regions (edges of window)
  float edgeSize = 40.0f * scale; // Size of dock zone at edges

  // Check if mouse is in a dock zone
  // Left zone
  if (mousePos.x >= absPos.x && mousePos.x <= absPos.x + edgeSize &&
      mousePos.y >= absPos.y && mousePos.y <= absPos.y + size.y) {
    DockHint hint;
    hint.Zone = DockZone::Left;
    hint.TargetWindow = const_cast<IWindow *>(this);
    hint.PreviewRect = glm::vec4(absPos.x, absPos.y, size.x * 0.5f, size.y);
    hint.IsValid = true;
    hints.push_back(hint);
  }

  // Right zone
  if (mousePos.x >= absPos.x + size.x - edgeSize &&
      mousePos.x <= absPos.x + size.x && mousePos.y >= absPos.y &&
      mousePos.y <= absPos.y + size.y) {
    DockHint hint;
    hint.Zone = DockZone::Right;
    hint.TargetWindow = const_cast<IWindow *>(this);
    hint.PreviewRect =
        glm::vec4(absPos.x + size.x * 0.5f, absPos.y, size.x * 0.5f, size.y);
    hint.IsValid = true;
    hints.push_back(hint);
  }

  // Top zone
  if (mousePos.y >= absPos.y && mousePos.y <= absPos.y + edgeSize &&
      mousePos.x >= absPos.x && mousePos.x <= absPos.x + size.x) {
    DockHint hint;
    hint.Zone = DockZone::Top;
    hint.TargetWindow = const_cast<IWindow *>(this);
    hint.PreviewRect = glm::vec4(absPos.x, absPos.y, size.x, size.y * 0.5f);
    hint.IsValid = true;
    hints.push_back(hint);
  }

  // Bottom zone
  if (mousePos.y >= absPos.y + size.y - edgeSize &&
      mousePos.y <= absPos.y + size.y && mousePos.x >= absPos.x &&
      mousePos.x <= absPos.x + size.x) {
    DockHint hint;
    hint.Zone = DockZone::Bottom;
    hint.TargetWindow = const_cast<IWindow *>(this);
    hint.PreviewRect =
        glm::vec4(absPos.x, absPos.y + size.y * 0.5f, size.x, size.y * 0.5f);
    hint.IsValid = true;
    hints.push_back(hint);
  }

  // Center zone (for tab docking) - if mouse is in center of title bar
  glm::vec4 titleBar = GetTitleBarRect();
  if (mousePos.x >= titleBar.x + edgeSize &&
      mousePos.x <= titleBar.x + titleBar.z - edgeSize &&
      mousePos.y >= titleBar.y && mousePos.y <= titleBar.y + titleBar.w) {
    DockHint hint;
    hint.Zone = DockZone::Center;
    hint.TargetWindow = const_cast<IWindow *>(this);
    hint.PreviewRect = glm::vec4(absPos.x, absPos.y, size.x, size.y);
    hint.IsValid = true;
    hints.push_back(hint);
  }

  return hints;
}

void IWindow::NotifyDragStart(const glm::vec2 &globalMouse) {
  // Find IDock in parent hierarchy and notify it
  UIControl *current = GetParent();
  while (current) {
    IDock *dock = dynamic_cast<IDock *>(current);
    if (dock) {
      AppUI *appUI = nullptr;
      // The IDock has m_AppUI which we can use
      // But it's private, so we need to go through the dock's public
      // interface Actually, let's traverse to find root and assume AppUI
      // created the dock
      break;
    }
    current = current->GetParent();
  }

  // Alternative: traverse to root and find the IDock
  UIControl *root = this;
  while (root->GetParent()) {
    root = root->GetParent();
  }

  // Check if root has an IDock as a child
  for (auto &child : root->GetChildren()) {
    IDock *dock = dynamic_cast<IDock *>(child.get());
    if (dock) {
      // The dock has access to AppUI, so call its UpdateDockPreview
      // directly
      dock->UpdateDockPreview(globalMouse, this);
      return;
    }
  }
}

void IWindow::NotifyDragEnd() {
  // Find IDock and clear preview
  UIControl *root = this;
  while (root->GetParent()) {
    root = root->GetParent();
  }

  for (auto &child : root->GetChildren()) {
    IDock *dock = dynamic_cast<IDock *>(child.get());
    if (dock) {
      dock->ClearDockPreview();
      return;
    }
  }
}

glm::vec2 IWindow::GetMinSize() const {
  float scale = AppUI::GetScale();
  float headerH =
      (!m_Tabs.empty() ? m_TabHeaderHeight : m_TitleBarHeight) * scale;

  // Minimal functional size: Header + 50px content
  glm::vec2 functionalMin(100.0f * scale, headerH + (50.0f * scale));

  // Use the larger of the user-set min size or functional min
  return glm::vec2(std::max(m_MinSize.x, functionalMin.x),
                   std::max(m_MinSize.y, functionalMin.y));
}

} // namespace Vivid
