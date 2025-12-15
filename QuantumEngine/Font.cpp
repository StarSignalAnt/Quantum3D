#include "Font.h"
#include "Texture2D.h"
#include "VividDevice.h"
#include "stb_truetype.h"
#include <fstream>
#include <stdexcept>


namespace Vivid {

Font::Font(VividDevice *device, const std::string &ttfPath, float fontSize)
    : m_DevicePtr(device), m_AtlasTexture(nullptr), m_FontSize(fontSize),
      m_LineHeight(0), m_Ascent(0), m_Descent(0), m_AtlasWidth(512),
      m_AtlasHeight(512) {
  LoadFont(ttfPath);
  CreateAtlas();
}

Font::~Font() { delete m_AtlasTexture; }

void Font::LoadFont(const std::string &ttfPath) {
  // Read TTF file
  std::ifstream file(ttfPath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open font file: " + ttfPath);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);

  m_FontData.resize(fileSize);
  if (!file.read(reinterpret_cast<char *>(m_FontData.data()), fileSize)) {
    throw std::runtime_error("Failed to read font file: " + ttfPath);
  }
}

void Font::CreateAtlas() {
  // Initialize stb_truetype
  stbtt_fontinfo fontInfo;
  if (!stbtt_InitFont(&fontInfo, m_FontData.data(), 0)) {
    throw std::runtime_error("Failed to initialize font");
  }

  // Get font metrics
  float scale = stbtt_ScaleForPixelHeight(&fontInfo, m_FontSize);
  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

  m_Ascent = ascent * scale;
  m_Descent = descent * scale;
  m_LineHeight = (ascent - descent + lineGap) * scale;

  // Create atlas bitmap
  std::vector<unsigned char> atlasBitmap(m_AtlasWidth * m_AtlasHeight, 0);

  // Pack characters into atlas
  int cursorX = 1;
  int cursorY = 1;
  int rowHeight = 0;

  // ASCII printable characters (32-126)
  for (int c = 32; c <= 126; c++) {
    int glyphWidth, glyphHeight, xoff, yoff;
    unsigned char *glyphBitmap = stbtt_GetCodepointBitmap(
        &fontInfo, 0, scale, c, &glyphWidth, &glyphHeight, &xoff, &yoff);

    if (!glyphBitmap) {
      continue;
    }

    // Check if we need to move to next row
    if (cursorX + glyphWidth + 1 >= m_AtlasWidth) {
      cursorX = 1;
      cursorY += rowHeight + 1;
      rowHeight = 0;
    }

    // Check if we've run out of space
    if (cursorY + glyphHeight + 1 >= m_AtlasHeight) {
      stbtt_FreeBitmap(glyphBitmap, nullptr);
      break;
    }

    // Copy glyph to atlas
    for (int y = 0; y < glyphHeight; y++) {
      for (int x = 0; x < glyphWidth; x++) {
        int atlasIdx = (cursorY + y) * m_AtlasWidth + (cursorX + x);
        atlasBitmap[atlasIdx] = glyphBitmap[y * glyphWidth + x];
      }
    }

    // Get advance width
    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidth, &leftSideBearing);

    // Store glyph info
    GlyphInfo glyph;
    glyph.u0 = static_cast<float>(cursorX) / m_AtlasWidth;
    glyph.v0 = static_cast<float>(cursorY) / m_AtlasHeight;
    glyph.u1 = static_cast<float>(cursorX + glyphWidth) / m_AtlasWidth;
    glyph.v1 = static_cast<float>(cursorY + glyphHeight) / m_AtlasHeight;
    glyph.xOffset = static_cast<float>(xoff);
    glyph.yOffset = static_cast<float>(yoff);
    glyph.xAdvance = advanceWidth * scale;
    glyph.width = static_cast<float>(glyphWidth);
    glyph.height = static_cast<float>(glyphHeight);

    m_Glyphs[static_cast<char>(c)] = glyph;

    cursorX += glyphWidth + 1;
    rowHeight = std::max(rowHeight, glyphHeight);

    stbtt_FreeBitmap(glyphBitmap, nullptr);
  }

  // Convert grayscale to RGBA for texture
  std::vector<unsigned char> atlasRGBA(m_AtlasWidth * m_AtlasHeight * 4);
  for (int i = 0; i < m_AtlasWidth * m_AtlasHeight; i++) {
    atlasRGBA[i * 4 + 0] = 255;            // R
    atlasRGBA[i * 4 + 1] = 255;            // G
    atlasRGBA[i * 4 + 2] = 255;            // B
    atlasRGBA[i * 4 + 3] = atlasBitmap[i]; // A
  }

  // Create texture from atlas
  m_AtlasTexture = new Texture2D(m_DevicePtr, atlasRGBA.data(), m_AtlasWidth,
                                 m_AtlasHeight, 4);
}

const GlyphInfo *Font::GetGlyph(char c) const {
  auto it = m_Glyphs.find(c);
  if (it != m_Glyphs.end()) {
    return &it->second;
  }
  // Return space for unknown characters
  it = m_Glyphs.find(' ');
  if (it != m_Glyphs.end()) {
    return &it->second;
  }
  return nullptr;
}

glm::vec2 Font::MeasureText(const std::string &text) const {
  float width = 0.0f;
  float height = m_LineHeight;

  for (char c : text) {
    if (c == '\n') {
      height += m_LineHeight;
      continue;
    }

    const GlyphInfo *glyph = GetGlyph(c);
    if (glyph) {
      width += glyph->xAdvance;
    }
  }

  return glm::vec2(width, height);
}

} // namespace Vivid
