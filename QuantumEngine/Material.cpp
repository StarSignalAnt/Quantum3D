#include "Material.h"
#include <array>
#include <iostream>

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

void Material::CreateDescriptorSet(
    Vivid::VividDevice *device, VkDescriptorPool pool,
    VkDescriptorSetLayout layout,
    std::shared_ptr<Vivid::Texture2D> defaultTexture, VkBuffer uboBuffer,
    VkDeviceSize uboSize, VkImageView shadowMapView,
    VkSampler shadowMapSampler) {
  // Skip if already created
  if (m_DescriptorSet != VK_NULL_HANDLE) {
    return;
  }

  // Allocate descriptor set from pool
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  VkResult result = vkAllocateDescriptorSets(device->GetDevice(), &allocInfo,
                                             &m_DescriptorSet);
  if (result != VK_SUCCESS) {
    std::cerr << "[Material] ERROR: Failed to allocate descriptor set for "
              << m_Name << "! VkResult: " << result << std::endl;
    return;
  }

  // Get textures with fallbacks to default
  Vivid::Texture2D *albedoTex =
      GetAlbedoTexture() ? GetAlbedoTexture().get() : defaultTexture.get();
  Vivid::Texture2D *normalTex =
      GetNormalTexture() ? GetNormalTexture().get() : defaultTexture.get();
  Vivid::Texture2D *metallicTex =
      GetMetallicTexture() ? GetMetallicTexture().get() : defaultTexture.get();
  Vivid::Texture2D *roughnessTex = GetRoughnessTexture()
                                       ? GetRoughnessTexture().get()
                                       : defaultTexture.get();

  // ========== Binding 0: UBO ==========
  VkDescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = uboBuffer;
  bufferInfo.offset = 0;
  bufferInfo.range = uboSize;

  // ========== Bindings 1-4: Textures ==========
  std::array<VkDescriptorImageInfo, 4> imageInfos{};
  imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfos[0].imageView = albedoTex->GetImageView();
  imageInfos[0].sampler = albedoTex->GetSampler();

  imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfos[1].imageView = normalTex->GetImageView();
  imageInfos[1].sampler = normalTex->GetSampler();

  imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfos[2].imageView = metallicTex->GetImageView();
  imageInfos[2].sampler = metallicTex->GetSampler();

  imageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfos[3].imageView = roughnessTex->GetImageView();
  imageInfos[3].sampler = roughnessTex->GetSampler();

  // ========== Binding 5: Shadow Cube Map ==========
  VkDescriptorImageInfo shadowImageInfo{};
  shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  shadowImageInfo.imageView = shadowMapView;
  shadowImageInfo.sampler = shadowMapSampler;

  // Create writes for all 6 bindings (UBO + 4 textures + shadow map)
  std::array<VkWriteDescriptorSet, 6> writes{};

  // UBO (binding 0)
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = nullptr;
  writes[0].dstSet = m_DescriptorSet;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &bufferInfo;

  // Textures (bindings 1-4)
  for (int i = 0; i < 4; ++i) {
    writes[1 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1 + i].pNext = nullptr;
    writes[1 + i].dstSet = m_DescriptorSet;
    writes[1 + i].dstBinding = 1 + i; // Bindings 1-4
    writes[1 + i].dstArrayElement = 0;
    writes[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1 + i].descriptorCount = 1;
    writes[1 + i].pImageInfo = &imageInfos[i];
  }

  // Shadow map (binding 5)
  writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[5].pNext = nullptr;
  writes[5].dstSet = m_DescriptorSet;
  writes[5].dstBinding = 5;
  writes[5].dstArrayElement = 0;
  writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[5].descriptorCount = 1;
  writes[5].pImageInfo = &shadowImageInfo;

  vkUpdateDescriptorSets(device->GetDevice(),
                         static_cast<uint32_t>(writes.size()), writes.data(), 0,
                         nullptr);

  std::cout << "[Material] Created descriptor set for " << m_Name << std::endl;
}

} // namespace Quantum
