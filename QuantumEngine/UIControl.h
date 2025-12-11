#pragma once

#include "AppInput.h"
#include "glm/glm.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Vivid {

class Draw2D;
class UIControl;
class UITheme;
class AppUI;

using UIControlPtr = std::shared_ptr<UIControl>;

class UIControl : public std::enable_shared_from_this<UIControl> {
  friend class AppUI; // Allow AppUI to access protected handlers (for Capture
                      // routing)
public:
  UIControl();
  virtual ~UIControl() = default;

  // Properties
  void SetPosition(const glm::vec2 &pos) { m_Position = pos; }
  void SetSize(const glm::vec2 &size) { m_Size = size; }
  void SetColor(const glm::vec4 &color) { m_Color = color; }
  void SetText(const std::string &text) { m_Text = text; }
  void SetVisible(bool visible) { m_Visible = visible; }
  void SetEnabled(bool enabled) { m_Enabled = enabled; }
  virtual void SetTheme(UITheme *theme) {
    m_Theme = theme;
    for (auto &child : m_Children) {
      if (child)
        child->SetTheme(theme);
    }
  }

  glm::vec2 GetPosition() const { return m_Position; }
  glm::vec2 GetSize()
      const; // Moved to CPP to avoid circular dependency or include AppUI here?
  // Actually, standard practice for inline is fine if we include AppUI.h
  // But circular dep AppUI <-> UIControl is risky.
  // Let's move GetSize implementation to .cpp or strict include.
  // AppUI includes UIControl. UIControl includes AppUI? Circular.
  // We should implement GetSize in .cpp.

  glm::vec4 GetColor() const { return m_Color; }
  const std::string &GetText() const { return m_Text; }
  bool IsVisible() const { return m_Visible; }
  bool IsEnabled() const { return m_Enabled; }
  bool IsHovered() const { return m_Hovered; }
  bool IsFocused() const { return m_Focused; }
  UITheme *GetTheme() const { return m_Theme; }

  // Clipping
  void SetClipsChildren(bool clips) { m_ClipsChildren = clips; }
  bool GetClipsChildren() const { return m_ClipsChildren; }
  glm::vec4 GetClipRect() const; // Returns scissor rect in pixels

  // Get absolute position (accounts for parent)
  glm::vec2 GetAbsolutePosition() const;

  // Hierarchy
  virtual void AddChild(UIControlPtr child);
  virtual void RemoveChild(UIControlPtr child);
  void MoveChildToFront(
      UIControl *child); // Moves specific child to end of list (render top)
  void MoveChildToBack(UIControl *child); // Moves specific child to start
  void ClearChildren();
  void ClearHoverState(); // Recursively clears hover state on this and children
  const std::vector<UIControlPtr> &GetChildren() const { return m_Children; }
  UIControl *GetParent() const { return m_Parent; }

  // Z-order helpers (for this control within parent)
  void BringToFront();   // Move this control to front of parent's children
  void SendToBack();     // Move this control to back of parent's children
  int GetZOrder() const; // Returns index in parent's children (-1 if no parent)

  // ID system for serialization/lookup
  void SetId(const std::string &id) { m_Id = id; }
  const std::string &GetId() const { return m_Id; }

  // Size constraints for docking/layout
  void SetMinSize(const glm::vec2 &minSize) { m_MinSize = minSize; }
  void SetMaxSize(const glm::vec2 &maxSize) { m_MaxSize = maxSize; }
  virtual glm::vec2 GetMinSize() const { return m_MinSize; }
  glm::vec2 GetMaxSize() const { return m_MaxSize; }

  // Hit testing
  bool Contains(const glm::vec2 &point) const;
  // Returns the top-most child at 'point'. Skips 'exclude' and its children.
  UIControl *GetControlAt(const glm::vec2 &point, UIControl *exclude = nullptr);

  // Update and rendering
  virtual void Update(float deltaTime);
  virtual void Draw(Draw2D *draw2D);

  // Input processing (returns true if handled)
  // outCapturedControl: Set to 'this' if OnMouseDown was triggered
  virtual bool ProcessInput(const AppInput &input, const glm::vec2 &mousePos,
                            UIControl *&outCapturedControl);

  // Passive input processing - for inputs like mouse wheel that work
  // regardless of focus/capture state. Called before regular ProcessInput.
  // Returns true if input was consumed (stops propagation to other controls).
  virtual bool ProcessPassiveInput(const AppInput &input,
                                   const glm::vec2 &mousePos);

protected:
  // Virtual event handlers - override in derived classes
  virtual void OnMouseEnter() {}
  virtual void OnMouseLeave() {}
  virtual void OnMouseDown(MouseButton button) { (void)button; }
  virtual void OnMouseUp(MouseButton button) { (void)button; }
  virtual void OnMouseMove(const glm::vec2 &position) { (void)position; }
  virtual void OnKeyDown(Key key) { (void)key; }
  virtual void OnKeyUp(Key key) { (void)key; }
  virtual void OnFocusGained() {}
  virtual void OnFocusLost() {}
  virtual void OnClick() {}

  // Drawing helpers
  virtual void OnDraw(Draw2D *draw2D) { (void)draw2D; }

  glm::vec2 m_Position;
  glm::vec2 m_Size;
  glm::vec4 m_Color;
  std::string m_Text;
  bool m_Visible;
  bool m_Enabled;
  bool m_Hovered;
  bool m_Focused;

  UIControl *m_Parent;
  UITheme *m_Theme;
  std::vector<UIControlPtr> m_Children;
  bool m_ClipsChildren;
  bool m_WasMouseDown;

  // ID and size constraints for docking infrastructure
  std::string m_Id;
  glm::vec2 m_MinSize;
  glm::vec2 m_MaxSize;
};

} // namespace Vivid
