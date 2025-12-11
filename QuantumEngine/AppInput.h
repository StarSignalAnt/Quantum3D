#pragma once

#include "glm/glm.hpp"
#include <array>

namespace Vivid {

// Key codes matching GLFW
enum class Key {
  Unknown = -1,
  Space = 32,
  Apostrophe = 39,
  Comma = 44,
  Minus = 45,
  Period = 46,
  Slash = 47,
  Num0 = 48,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,
  Semicolon = 59,
  Equal = 61,
  A = 65,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  LeftBracket = 91,
  Backslash = 92,
  RightBracket = 93,
  GraveAccent = 96,
  Escape = 256,
  Enter = 257,
  Tab = 258,
  Backspace = 259,
  Insert = 260,
  Delete = 261,
  Right = 262,
  Left = 263,
  Down = 264,
  Up = 265,
  PageUp = 266,
  PageDown = 267,
  Home = 268,
  End = 269,
  CapsLock = 280,
  ScrollLock = 281,
  NumLock = 282,
  PrintScreen = 283,
  Pause = 284,
  F1 = 290,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  LeftShift = 340,
  LeftControl = 341,
  LeftAlt = 342,
  LeftSuper = 343,
  RightShift = 344,
  RightControl = 345,
  RightAlt = 346,
  RightSuper = 347,
  Menu = 348,
  MaxKeys = 512
};

enum class MouseButton {
  Left = 0,
  Right = 1,
  Middle = 2,
  Button4 = 3,
  Button5 = 4,
  MaxButtons = 8
};

class AppInput {
public:
  AppInput();

  // Called by VividApplication to update state
  void Update();
  void SetMousePosition(float x, float y);
  void SetMouseButton(MouseButton button, bool pressed);
  void SetKey(Key key, bool pressed);
  void SetMouseScroll(float xOffset, float yOffset);

  // Query current state
  glm::vec2 GetMousePosition() const { return m_MousePosition; }
  glm::vec2 GetMouseDelta() const { return m_MouseDelta; }
  glm::vec2 GetScrollDelta() const { return m_ScrollDelta; }

  bool IsMouseButtonDown(MouseButton button) const;
  bool
  IsMouseButtonPressed(MouseButton button) const; // Just pressed this frame
  bool
  IsMouseButtonReleased(MouseButton button) const; // Just released this frame

  bool IsKeyDown(Key key) const;
  bool IsKeyPressed(Key key) const;  // Just pressed this frame
  bool IsKeyReleased(Key key) const; // Just released this frame

private:
  glm::vec2 m_MousePosition;
  glm::vec2 m_LastMousePosition;
  glm::vec2 m_MouseDelta;
  glm::vec2 m_ScrollDelta;

  std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> m_MouseButtons;
  std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)>
      m_LastMouseButtons;

  std::array<bool, static_cast<size_t>(Key::MaxKeys)> m_Keys;
  std::array<bool, static_cast<size_t>(Key::MaxKeys)> m_LastKeys;
};

} // namespace Vivid
