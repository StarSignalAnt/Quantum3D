#pragma once

#include "VividDevice.h"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace Vivid {

struct BlendConfig {
  VkBool32 blendEnable = VK_TRUE;
  VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
};

enum class PipelineType {
  Sprite2D, // 2D sprite/UI pipeline with instance data
  Mesh3D    // 3D mesh pipeline with Vertex3D data
};

class VividPipeline {
public:
  // Original 2D sprite pipeline constructor (default)
  VividPipeline(VividDevice *device, const std::string &vertPath,
                const std::string &fragPath,
                VkDescriptorSetLayout descriptorSetLayout,
                VkRenderPass renderPass,
                const BlendConfig &blendConfig = BlendConfig{});

  // Extended constructor with pipeline type
  VividPipeline(VividDevice *device, const std::string &vertPath,
                const std::string &fragPath,
                VkDescriptorSetLayout descriptorSetLayout,
                VkRenderPass renderPass, const BlendConfig &blendConfig,
                PipelineType pipelineType);

  ~VividPipeline();

  VkPipeline GetPipeline() const { return m_Pipeline; }
  VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
  void Bind(VkCommandBuffer commandBuffer);

private:
  void CreatePipeline(const std::string &vertPath, const std::string &fragPath,
                      VkDescriptorSetLayout descriptorSetLayout,
                      VkRenderPass renderPass, const BlendConfig &blendConfig,
                      PipelineType pipelineType);

  static std::vector<char> ReadFile(const std::string &filename);
  VkShaderModule CreateShaderModule(const std::vector<char> &code);

  VividDevice *m_DevicePtr;
  VkPipeline m_Pipeline;
  VkPipelineLayout m_PipelineLayout;
};
} // namespace Vivid
