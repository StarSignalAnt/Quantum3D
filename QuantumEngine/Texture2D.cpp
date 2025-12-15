#include "Texture2D.h"
#include "VividBuffer.h"
#include "pch.h"
#include "stb_image.h"
#include <iostream>
#include <stdexcept>

namespace Vivid {
Texture2D::Texture2D(VividDevice *device, const std::string &path,
                     VkFormat format)
    : m_DevicePtr(device), m_Format(format) {
  CreateTextureImage(path);
  CreateTextureImageView();
  CreateTextureSampler();
}

Texture2D::Texture2D(VividDevice *device, const unsigned char *pixels,
                     int width, int height, int channels, VkFormat format)
    : m_DevicePtr(device), m_Width(width), m_Height(height),
      m_Channels(channels), m_Format(format) {
  CreateTextureImageFromData(pixels, width, height, channels);
  CreateTextureImageView();
  CreateTextureSampler();
}

Texture2D::Texture2D(VividDevice *device, VkImageView view, VkSampler sampler,
                     int width, int height)
    : m_DevicePtr(device), m_TextureImageView(view), m_TextureSampler(sampler),
      m_Width(width), m_Height(height), m_OwnsResources(false) {}

Texture2D::~Texture2D() {
  // Check if device pointer is still valid before cleanup
  if (m_DevicePtr && m_DevicePtr->GetDevice() != VK_NULL_HANDLE &&
      m_OwnsResources) {
    if (m_TextureSampler != VK_NULL_HANDLE) {
      vkDestroySampler(m_DevicePtr->GetDevice(), m_TextureSampler, nullptr);
    }
    if (m_TextureImageView != VK_NULL_HANDLE) {
      vkDestroyImageView(m_DevicePtr->GetDevice(), m_TextureImageView, nullptr);
    }
    if (m_TextureImage != VK_NULL_HANDLE) {
      vkDestroyImage(m_DevicePtr->GetDevice(), m_TextureImage, nullptr);
    }
    if (m_TextureImageMemory != VK_NULL_HANDLE) {
      vkFreeMemory(m_DevicePtr->GetDevice(), m_TextureImageMemory, nullptr);
    }
  }
}

void Texture2D::CreateTextureImage(const std::string &path) {
  stbi_set_flip_vertically_on_load(false);

  stbi_uc *pixels =
      stbi_load(path.c_str(), &m_Width, &m_Height, &m_Channels, STBI_rgb_alpha);
  VkDeviceSize imageSize = m_Width * m_Height * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image: " + path);
  }

  VividBuffer stagingBuffer(m_DevicePtr, imageSize,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  stagingBuffer.WriteToBuffer(pixels);

  stbi_image_free(pixels);

  m_DevicePtr->CreateImage(m_Width, m_Height, m_Format, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_TextureImage,
                           m_TextureImageMemory);

  m_DevicePtr->TransitionImageLayout(m_TextureImage, m_Format,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  m_DevicePtr->CopyBufferToImage(stagingBuffer.GetBuffer(), m_TextureImage,
                                 static_cast<uint32_t>(m_Width),
                                 static_cast<uint32_t>(m_Height));
  m_DevicePtr->TransitionImageLayout(m_TextureImage, m_Format,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Texture2D::CreateTextureImageFromData(const unsigned char *pixels,
                                           int width, int height,
                                           int channels) {
  VkDeviceSize imageSize = width * height * 4;

  VividBuffer stagingBuffer(m_DevicePtr, imageSize,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  stagingBuffer.WriteToBuffer(const_cast<unsigned char *>(pixels));

  m_DevicePtr->CreateImage(width, height, m_Format, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_TextureImage,
                           m_TextureImageMemory);

  m_DevicePtr->TransitionImageLayout(m_TextureImage, m_Format,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  m_DevicePtr->CopyBufferToImage(stagingBuffer.GetBuffer(), m_TextureImage,
                                 static_cast<uint32_t>(width),
                                 static_cast<uint32_t>(height));
  m_DevicePtr->TransitionImageLayout(m_TextureImage, m_Format,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Texture2D::CreateTextureImageView() {
  m_TextureImageView = m_DevicePtr->CreateImageView(m_TextureImage, m_Format);
}

void Texture2D::CreateTextureSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(m_DevicePtr->GetPhysicalDevice(), &properties);
  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(m_DevicePtr->GetDevice(), &samplerInfo, nullptr,
                      &m_TextureSampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture sampler!");
  }
}

VkDescriptorSet Texture2D::GetDescriptorSet(VkDescriptorPool pool,
                                            VkDescriptorSetLayout layout) {
  if (m_DescriptorSet != VK_NULL_HANDLE) {
    return m_DescriptorSet;
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  if (vkAllocateDescriptorSets(m_DevicePtr->GetDevice(), &allocInfo,
                               &m_DescriptorSet) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }

  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = m_TextureImageView;
  imageInfo.sampler = m_TextureSampler;

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = m_DescriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(m_DevicePtr->GetDevice(), 1, &descriptorWrite, 0,
                         nullptr);

  return m_DescriptorSet;
}
} // namespace Vivid
