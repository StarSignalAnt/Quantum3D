#pragma once

#include "VividDevice.h"
#include <string>
#include <vulkan/vulkan.h>

namespace Vivid {
class Texture2D {
public:
  // Load from file
  Texture2D(VividDevice *device, const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

  // Create from raw pixel data
  Texture2D(VividDevice *device, const unsigned char *pixels, int width,
            int height, int channels, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

  ~Texture2D();

  VkImageView GetImageView() const { return m_TextureImageView; }
  VkSampler GetSampler() const { return m_TextureSampler; }
  VkDescriptorSet GetDescriptorSet(VkDescriptorPool pool,
                                   VkDescriptorSetLayout layout);

  int GetWidth() const { return m_Width; }
  int GetHeight() const { return m_Height; }

private:
  void CreateTextureImage(const std::string &path);
  void CreateTextureImageFromData(const unsigned char *pixels, int width,
                                  int height, int channels);
  void CreateTextureImageView();
  void CreateTextureSampler();

  VividDevice *m_DevicePtr;

  VkImage m_TextureImage;
  VkDeviceMemory m_TextureImageMemory;
  VkImageView m_TextureImageView;
  VkSampler m_TextureSampler;
  VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
  VkFormat m_Format;

  int m_Width, m_Height, m_Channels;
};
} // namespace Vivid
