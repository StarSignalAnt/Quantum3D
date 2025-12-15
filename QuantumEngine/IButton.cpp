#include "IButton.h"
#include "AppUI.h"
#include "Draw2D.h"
#include "UITheme.h"

namespace Vivid {

IButton::IButton()
    : m_NormalColor(0.3f, 0.3f, 0.5f, 1.0f),
      m_HoverColor(0.6f, 0.6f, 1.0f, 1.0f),
      m_PressedColor(0.2f, 0.2f, 0.4f, 1.0f),
      m_DisabledColor(0.2f, 0.2f, 0.2f, 0.5f), m_Pressed(false),
      m_OnClickCallback(nullptr) {
  SetText("Button");
  SetSize(glm::vec2(120.0f, 32.0f));
  UpdateColor();
}

IButton::IButton(const std::string &text)
    : m_NormalColor(0.5f, 0.5f, 0.5f, 1.0f),
      m_HoverColor(0.95f, 0.95f, 1.0f, 1.0f),
      m_PressedColor(0.2f, 0.2f, 0.4f, 1.0f),
      m_DisabledColor(0.2f, 0.2f, 0.2f, 0.5f), m_Pressed(false),
      m_OnClickCallback(nullptr) {
  SetText(text);
  SetSize(glm::vec2(120.0f, 32.0f));
  UpdateColor();
}

void IButton::UpdateColor() {
  if (!IsEnabled()) {
    SetColor(m_DisabledColor);
  } else if (m_Pressed) {
    SetColor(m_PressedColor);
  } else if (IsHovered()) {
    SetColor(m_HoverColor);
  } else {
    SetColor(m_NormalColor);
  }
}

void IButton::OnMouseEnter() { UpdateColor(); }

void IButton::OnMouseLeave() {
  m_Pressed = false;
  UpdateColor();
}

void IButton::OnMouseDown(MouseButton button) {
  if (button == MouseButton::Left) {
    m_Pressed = true;
    UpdateColor();
  }
}

void IButton::OnMouseUp(MouseButton button) {
  if (button == MouseButton::Left) {
    m_Pressed = false;
    UpdateColor();
  }
}

void IButton::OnClick() {
  if (m_OnClickCallback) {
    m_OnClickCallback();
  }
}

void IButton::OnDraw(Draw2D *draw2D) {
  if (!draw2D)
    return;

  // Get theme texture for frame
  Texture2D *frameTexture = nullptr;
  Font *font = nullptr;
  if (m_Theme) {
    frameTexture = m_Theme->GetFrameTexture();
    font = m_Theme->GetFont();
  }

  glm::vec2 absPos = GetAbsolutePosition();
  glm::vec2 size = GetSize();

  if (frameTexture) {
    // Draw button using theme's frame texture with button's color
    draw2D->DrawTexture(absPos, size, frameTexture, GetColor(),
                        BlendMode::Alpha);
  }

  // Draw centered text
  const std::string &text = GetText();
  if (font && !text.empty()) {
    // Measure text to center it (MeasureText returns logical size, need to
    // scale)
    float scale = AppUI::GetScale();
    glm::vec2 textSize = font->MeasureText(text) * scale;

    // Calculate centered position
    float textX = absPos.x + (size.x - textSize.x) * 0.5f;
    float textY = absPos.y + (size.y - textSize.y) * 0.5f;

    // Draw text in white (or use theme foreground color)
    glm::vec4 textColor = m_Theme ? m_Theme->GetForegroundColor()
                                  : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    draw2D->RenderText(glm::vec2(textX, textY), text, font, textColor,
                       BlendMode::Alpha);
  }
}

} // namespace Vivid
