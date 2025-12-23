#pragma once
#include "VividDevice.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <vulkan/vulkan.h>

namespace Quantum {

/// <summary>
/// Manages a 2D shadow map for directional light shadows.
/// Creates a 2D depth texture for capturing depth from a directional source.
/// </summary>
class DirectionalShadowMap {
public:
  DirectionalShadowMap() = default;
  ~DirectionalShadowMap();

  // Initialize shadow map resources
  void Initialize(Vivid::VividDevice *device, uint32_t resolution = 2048);

  // Cleanup resources
  void Shutdown();

  // Get resolution
  uint32_t GetResolution() const { return m_Resolution; }

  // Get light space matrix (Ortho-Projection * View)
  glm::mat4
  GetLightSpaceMatrix(const glm::vec3 &lightDir,
                      const glm::vec3 &target = glm::vec3(0.0f)) const;

  // Get the image view for shader sampling
  VkImageView GetImageView() const { return m_ImageView; }

  // Get the sampler for shader sampling
  VkSampler GetSampler() const { return m_Sampler; }

  // Get framebuffer
  VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }

  // Check if initialized
  bool IsInitialized() const { return m_Initialized; }

  // Get render pass
  VkRenderPass GetRenderPass() const { return m_RenderPass; }

private:
  void CreateImage();
  void CreateImageView();
  void CreateSampler();
  void CreateRenderPass();
  void CreateFramebuffer();

  Vivid::VividDevice *m_Device = nullptr;
  uint32_t m_Resolution = 2048;
  bool m_Initialized = false;

  // 2D depth image
  VkImage m_Image = VK_NULL_HANDLE;
  VkDeviceMemory m_Memory = VK_NULL_HANDLE;
  VkImageView m_ImageView = VK_NULL_HANDLE;

  // Sampler for shadow comparison
  VkSampler m_Sampler = VK_NULL_HANDLE;

  // Render pass for shadow depth
  VkRenderPass m_RenderPass = VK_NULL_HANDLE;

  // Framebuffer
  VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
};

} // namespace Quantum
