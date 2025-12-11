#pragma once

#include "Texture2D.h"
#include "VividDevice.h"
#include "glm/glm.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace Vivid {

// Character glyph info
struct GlyphInfo {
  float u0, v0, u1, v1;   // Texture coordinates
  float xOffset, yOffset; // Offset from cursor when rendering
  float xAdvance;         // How much to advance cursor after this char
  float width, height;    // Glyph dimensions in pixels
};

class Font {
public:
  Font(VividDevice *device, const std::string &ttfPath, float fontSize);
  ~Font();

  // Get glyph info for a character
  const GlyphInfo *GetGlyph(char c) const;

  // Get the font atlas texture
  Texture2D *GetAtlasTexture() const { return m_AtlasTexture; }

  // Font metrics
  float GetFontSize() const { return m_FontSize; }
  float GetLineHeight() const { return m_LineHeight; }
  float GetAscent() const { return m_Ascent; }
  float GetDescent() const { return m_Descent; }

  // Measure text dimensions
  glm::vec2 MeasureText(const std::string &text) const;

private:
  void LoadFont(const std::string &ttfPath);
  void CreateAtlas();

  VividDevice *m_DevicePtr;
  Texture2D *m_AtlasTexture;
  float m_FontSize;
  float m_LineHeight;
  float m_Ascent;
  float m_Descent;

  int m_AtlasWidth;
  int m_AtlasHeight;

  std::unordered_map<char, GlyphInfo> m_Glyphs;
  std::vector<unsigned char> m_FontData;
};

} // namespace Vivid
