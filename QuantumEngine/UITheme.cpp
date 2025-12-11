#include "UITheme.h"

namespace Vivid {

UITheme::UITheme()
    : m_BackgroundColor(0.2f, 0.2f, 0.2f, 1.0f),
      m_ForegroundColor(1.0f, 1.0f, 1.0f, 1.0f),
      m_WindowBackgroundColor(0.25f, 0.25f, 0.25f, 1.0f),
      m_TitleBarColor(0.1f, 0.1f, 0.1f, 1.0f),
      m_TitleTextColor(1.0f, 1.0f, 1.0f, 1.0f),
      m_BorderColor(0.4f, 0.4f, 0.4f, 1.0f),
      m_TabActiveColor(0.25f, 0.25f, 0.25f, 1.0f),
      m_TabInactiveColor(0.15f, 0.15f, 0.15f, 1.0f), m_FrameTexture(nullptr),
      m_Font(nullptr), m_DevicePtr(nullptr) {}

UITheme::~UITheme() {
  delete m_FrameTexture;
  delete m_Font;
}

} // namespace Vivid
