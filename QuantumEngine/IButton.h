#pragma once

#include "UIControl.h"
#include <functional>
#include <string>

namespace Vivid {

class IButton : public UIControl {
public:
  IButton();
  IButton(const std::string &text);
  virtual ~IButton() = default;

  // Colors
  void SetNormalColor(const glm::vec4 &color) { m_NormalColor = color; }
  void SetHoverColor(const glm::vec4 &color) { m_HoverColor = color; }
  void SetPressedColor(const glm::vec4 &color) { m_PressedColor = color; }
  void SetDisabledColor(const glm::vec4 &color) { m_DisabledColor = color; }

  glm::vec4 GetNormalColor() const { return m_NormalColor; }
  glm::vec4 GetHoverColor() const { return m_HoverColor; }
  glm::vec4 GetPressedColor() const { return m_PressedColor; }
  glm::vec4 GetDisabledColor() const { return m_DisabledColor; }

  // Click callback
  using ClickCallback = std::function<void()>;
  void SetOnClick(ClickCallback callback) { m_OnClickCallback = callback; }

  // State
  bool IsPressed() const { return m_Pressed; }

protected:
  void OnMouseEnter() override;
  void OnMouseLeave() override;
  void OnMouseDown(MouseButton button) override;
  void OnMouseUp(MouseButton button) override;
  void OnClick() override;
  void OnDraw(Draw2D *draw2D) override;

  void UpdateColor();

  glm::vec4 m_NormalColor;
  glm::vec4 m_HoverColor;
  glm::vec4 m_PressedColor;
  glm::vec4 m_DisabledColor;
  bool m_Pressed;
  ClickCallback m_OnClickCallback;
};

} // namespace Vivid
