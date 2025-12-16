#pragma once

#include "PipelineTypes.h"
#include "VividDevice.h"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace Vivid {

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

  // Constructor with multiple descriptor set layouts
  VividPipeline(VividDevice *device, const std::string &vertPath,
                const std::string &fragPath,
                const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts,
                VkRenderPass renderPass, const BlendConfig &blendConfig,
                PipelineType pipelineType);

  ~VividPipeline();

  VkPipeline GetPipeline() const { return m_Pipeline; }
  VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
  void Bind(VkCommandBuffer commandBuffer);

private:
  void
  CreatePipeline(const std::string &vertPath, const std::string &fragPath,
                 const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts,
                 VkRenderPass renderPass, const BlendConfig &blendConfig,
                 PipelineType pipelineType);

  static std::vector<char> ReadFile(const std::string &filename);
  VkShaderModule CreateShaderModule(const std::vector<char> &code);

  VividDevice *m_DevicePtr;
  VkPipeline m_Pipeline;
  VkPipelineLayout m_PipelineLayout;
};
} // namespace Vivid
