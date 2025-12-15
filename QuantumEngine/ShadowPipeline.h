#pragma once

#include "VividDevice.h"
#include "glm/glm.hpp"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace Quantum {

// Push constants for shadow depth rendering
struct ShadowPushConstants {
  glm::mat4 lightSpaceMatrix; // 64 bytes
  glm::mat4 model;            // 64 bytes
  glm::vec4 lightPos;         // 16 bytes (xyz = pos, w = farPlane)
  // Total: 144 bytes used, but structure aligned
};
// Total: 144 bytes

// Specialized pipeline for shadow depth rendering
// Uses larger push constants than the standard VividPipeline
class ShadowPipeline {
public:
  ShadowPipeline(Vivid::VividDevice *device, const std::string &vertPath,
                 const std::string &fragPath, VkRenderPass renderPass);
  ~ShadowPipeline();

  // Non-copyable
  ShadowPipeline(const ShadowPipeline &) = delete;
  ShadowPipeline &operator=(const ShadowPipeline &) = delete;

  VkPipeline GetPipeline() const { return m_Pipeline; }
  VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

  void Bind(VkCommandBuffer commandBuffer);

private:
  void CreatePipeline(const std::string &vertPath, const std::string &fragPath,
                      VkRenderPass renderPass);

  static std::vector<char> ReadShaderFile(const std::string &filename);
  VkShaderModule CreateShaderModule(const std::vector<char> &code);

  Vivid::VividDevice *m_Device;
  VkPipeline m_Pipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
};

} // namespace Quantum
