#include "ThemeDarkUI.h"

namespace Vivid {

ThemeDarkUI::ThemeDarkUI() {
  // Dark theme colors
  m_BackgroundColor = glm::vec4(0.15f, 0.15f, 0.18f, 1.0f); // Dark gray-blue
  m_ForegroundColor = glm::vec4(0.9f, 0.9f, 0.92f, 1.0f);   // Light gray-white

  // Professional Window Style
  m_WindowBackgroundColor =
      glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);               // Darker client area
  m_TitleBarColor = glm::vec4(0.1f, 0.1f, 0.12f, 1.0f); // Darker title bar
  m_TitleTextColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // White title text
  m_BorderColor = glm::vec4(0.3f, 0.3f, 0.35f, 1.0f);   // Subtle border

  m_TabActiveColor =
      glm::vec4(0.5f, 0.5f, 0.55f, 1.0f); // Much brighter highlight
  m_TabInactiveColor =
      glm::vec4(0.12f, 0.12f, 0.14f, 1.0f); // Darker inactive tab
}

void ThemeDarkUI::Init(VividDevice *device) {
  // Load standard UI font
  m_Font = new Font(device, "ui/fonts/arial.ttf", 14.0f); // Correct args
  m_DevicePtr = device;
  if (m_Font) {
    // Font constructor loads atlas
  }

  // Create a 1x1 white texture for solid color rendering
  unsigned char whitePixel[4] = {255, 255, 255, 255};
  m_WhiteTexture = new Texture2D(device, whitePixel, 1, 1, 4);
  m_FrameTexture = m_WhiteTexture; // Default to white

  // Try to load header gradient
  m_HeaderTexture =
      new Texture2D(device, "ui/themes/darkUI/ui_header_grad.png");

  // Keep frame texture as gradient for buttons? No, buttons use Frame.
  // Actually, original code overwrote m_FrameTexture with Header.
  // m_FrameTexture = m_HeaderTexture; // This was overwriting white texture
  // usage. Let's keep m_FrameTexture as White for now unless Buttons need
  // Gradient. Code review: Buttons look better with Gradient? User Issue: Ghost
  // tab needs WHITE to be bright. So we expose GetWhiteTexture() explicitly. I
  // will revert m_FrameTexture to m_HeaderTexture (Gradient) if that's the
  // style, BUT ensure m_WhiteTexture is stored.

  if (m_HeaderTexture) {
    m_FrameTexture = m_HeaderTexture;
  }
}

} // namespace Vivid
