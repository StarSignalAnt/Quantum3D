#include "Material.h"

namespace Quantum {

Material::Material(const std::string &name)
    : m_Name(name), m_PipelineName("PLPBR") {} // Default to PLPBR pipeline

Material::~Material() { m_Textures.clear(); }

void Material::SetPipeline(const std::string &pipelineName) {
  m_PipelineName = pipelineName;
}

Vivid::VividPipeline *Material::GetPipeline() const {
  if (m_PipelineName.empty()) {
    return nullptr;
  }
  return RenderingPipelines::Get().GetPipeline(m_PipelineName);
}

// ========== Generic Texture Management ==========

void Material::SetTexture(const std::string &slot,
                          std::shared_ptr<Vivid::Texture2D> texture) {
  if (texture) {
    m_Textures[slot] = texture;
  } else {
    m_Textures.erase(slot);
  }
}

std::shared_ptr<Vivid::Texture2D>
Material::GetTexture(const std::string &slot) const {
  auto it = m_Textures.find(slot);
  if (it != m_Textures.end()) {
    return it->second;
  }
  return nullptr;
}

bool Material::HasTexture(const std::string &slot) const {
  return m_Textures.find(slot) != m_Textures.end();
}

void Material::RemoveTexture(const std::string &slot) {
  m_Textures.erase(slot);
}

// ========== Common Texture Slot Helpers ==========

void Material::SetAlbedoTexture(std::shared_ptr<Vivid::Texture2D> texture) {
  SetTexture(SLOT_ALBEDO, texture);
}

void Material::SetNormalTexture(std::shared_ptr<Vivid::Texture2D> texture) {
  SetTexture(SLOT_NORMAL, texture);
}

void Material::SetMetallicTexture(std::shared_ptr<Vivid::Texture2D> texture) {
  SetTexture(SLOT_METALLIC, texture);
}

void Material::SetRoughnessTexture(std::shared_ptr<Vivid::Texture2D> texture) {
  SetTexture(SLOT_ROUGHNESS, texture);
}

void Material::SetAOTexture(std::shared_ptr<Vivid::Texture2D> texture) {
  SetTexture(SLOT_AO, texture);
}

void Material::SetEmissiveTexture(std::shared_ptr<Vivid::Texture2D> texture) {
  SetTexture(SLOT_EMISSIVE, texture);
}

void Material::CheckRequiredTextures(Vivid::VividDevice *device) {
  // 1x1 Default Pixel Data - optimized for visible PBR lighting
  unsigned char lightGreyPixel[] = {180, 180, 180,
                                    255}; // Pleasant neutral grey albedo
  unsigned char flatNormalPixel[] = {128, 128, 255,
                                     255};     // Flat normal (0, 0, 1)
  unsigned char blackPixel[] = {0, 0, 0, 255}; // Non-metallic
  unsigned char midRoughnessPixel[] = {
      128, 128, 128, 255}; // 0.5 roughness for visible specular
  unsigned char whitePixel[] = {255, 255, 255, 255}; // Fully lit (AO)

  // Albedo - Default Light Grey (more visible than pure white)
  if (m_Textures.find(SLOT_ALBEDO) == m_Textures.end()) {
    auto texture =
        std::make_shared<Vivid::Texture2D>(device, lightGreyPixel, 1, 1, 4);
    m_Textures[SLOT_ALBEDO] = texture;
  }

  // Normal - Default Flat Normal (0, 0, 1) -> (128, 128, 255)
  if (m_Textures.find(SLOT_NORMAL) == m_Textures.end()) {
    auto texture = std::make_shared<Vivid::Texture2D>(
        device, flatNormalPixel, 1, 1, 4, VK_FORMAT_R8G8B8A8_UNORM);
    m_Textures[SLOT_NORMAL] = texture;
  }

  // Metallic - Default Non-metallic (Black = 0.0)
  if (m_Textures.find(SLOT_METALLIC) == m_Textures.end()) {
    auto texture = std::make_shared<Vivid::Texture2D>(
        device, blackPixel, 1, 1, 4, VK_FORMAT_R8G8B8A8_UNORM);
    m_Textures[SLOT_METALLIC] = texture;
  }

  // Roughness - Default Mid-Range (0.5 for visible specular highlights)
  if (m_Textures.find(SLOT_ROUGHNESS) == m_Textures.end()) {
    auto texture = std::make_shared<Vivid::Texture2D>(
        device, midRoughnessPixel, 1, 1, 4, VK_FORMAT_R8G8B8A8_UNORM);
    m_Textures[SLOT_ROUGHNESS] = texture;
  }

  // AO - Default Fully Lit (White = 1.0)
  if (m_Textures.find(SLOT_AO) == m_Textures.end()) {
    auto texture = std::make_shared<Vivid::Texture2D>(
        device, whitePixel, 1, 1, 4, VK_FORMAT_R8G8B8A8_UNORM);
    m_Textures[SLOT_AO] = texture;
  }
}

std::shared_ptr<Vivid::Texture2D> Material::GetAlbedoTexture() const {
  return GetTexture(SLOT_ALBEDO);
}

std::shared_ptr<Vivid::Texture2D> Material::GetNormalTexture() const {
  return GetTexture(SLOT_NORMAL);
}

std::shared_ptr<Vivid::Texture2D> Material::GetMetallicTexture() const {
  return GetTexture(SLOT_METALLIC);
}

std::shared_ptr<Vivid::Texture2D> Material::GetRoughnessTexture() const {
  return GetTexture(SLOT_ROUGHNESS);
}

std::shared_ptr<Vivid::Texture2D> Material::GetAOTexture() const {
  return GetTexture(SLOT_AO);
}

std::shared_ptr<Vivid::Texture2D> Material::GetEmissiveTexture() const {
  return GetTexture(SLOT_EMISSIVE);
}

} // namespace Quantum
