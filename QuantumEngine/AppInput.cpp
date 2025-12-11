#include "AppInput.h"

namespace Vivid {

AppInput::AppInput()
    : m_MousePosition(0.0f), m_LastMousePosition(0.0f), m_MouseDelta(0.0f),
      m_ScrollDelta(0.0f) {
  m_MouseButtons.fill(false);
  m_LastMouseButtons.fill(false);
  m_Keys.fill(false);
  m_LastKeys.fill(false);
}

void AppInput::Update() {
  // Store last frame state
  m_LastMouseButtons = m_MouseButtons;
  m_LastKeys = m_Keys;

  // Calculate mouse delta
  m_MouseDelta = m_MousePosition - m_LastMousePosition;
  m_LastMousePosition = m_MousePosition;

  // Reset scroll delta (only valid for one frame)
  m_ScrollDelta = glm::vec2(0.0f);
}

void AppInput::SetMousePosition(float x, float y) {
  m_MousePosition = glm::vec2(x, y);
}

void AppInput::SetMouseButton(MouseButton button, bool pressed) {
  size_t index = static_cast<size_t>(button);
  if (index < m_MouseButtons.size()) {
    m_MouseButtons[index] = pressed;
  }
}

void AppInput::SetKey(Key key, bool pressed) {
  int index = static_cast<int>(key);
  if (index >= 0 && index < static_cast<int>(m_Keys.size())) {
    m_Keys[index] = pressed;
  }
}

void AppInput::SetMouseScroll(float xOffset, float yOffset) {
  m_ScrollDelta = glm::vec2(xOffset, yOffset);
}

bool AppInput::IsMouseButtonDown(MouseButton button) const {
  size_t index = static_cast<size_t>(button);
  return index < m_MouseButtons.size() && m_MouseButtons[index];
}

bool AppInput::IsMouseButtonPressed(MouseButton button) const {
  size_t index = static_cast<size_t>(button);
  return index < m_MouseButtons.size() && m_MouseButtons[index] &&
         !m_LastMouseButtons[index];
}

bool AppInput::IsMouseButtonReleased(MouseButton button) const {
  size_t index = static_cast<size_t>(button);
  return index < m_MouseButtons.size() && !m_MouseButtons[index] &&
         m_LastMouseButtons[index];
}

bool AppInput::IsKeyDown(Key key) const {
  int index = static_cast<int>(key);
  return index >= 0 && index < static_cast<int>(m_Keys.size()) && m_Keys[index];
}

bool AppInput::IsKeyPressed(Key key) const {
  int index = static_cast<int>(key);
  return index >= 0 && index < static_cast<int>(m_Keys.size()) &&
         m_Keys[index] && !m_LastKeys[index];
}

bool AppInput::IsKeyReleased(Key key) const {
  int index = static_cast<int>(key);
  return index >= 0 && index < static_cast<int>(m_Keys.size()) &&
         !m_Keys[index] && m_LastKeys[index];
}

} // namespace Vivid
