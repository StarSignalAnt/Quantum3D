#pragma once
#include "PipelineTypes.h"
#include "VividDevice.h"
#include <memory>

namespace Vivid {
class VividPipeline;
}
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace Quantum {

/// <summary>
/// Singleton that manages rendering pipelines.
/// Pipelines are unique per shader pair - multiple materials can share the same
/// pipeline but use different textures/uniforms.
///
/// Usage: auto* pipeline = RenderingPipelines::Get().GetPipeline("PBR");
/// </summary>
class RenderingPipelines {
public:
  // Singleton access
  static RenderingPipelines &Get();

  // Delete copy/move
  RenderingPipelines(const RenderingPipelines &) = delete;
  RenderingPipelines &operator=(const RenderingPipelines &) = delete;

  /// <summary>
  /// Initialize the pipeline manager with the Vulkan device.
  /// Must be called before using any pipelines.
  /// </summary>
  void
  Initialize(Vivid::VividDevice *device, VkRenderPass renderPass,
             const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts);

  /// <summary>
  /// Shutdown and cleanup all pipelines.
  /// </summary>
  void Shutdown();

  /// <summary>
  /// Invalidate all created pipelines (for swapchain recreation).
  /// Keeps registrations so pipelines can be lazily recreated.
  /// </summary>
  void InvalidatePipelines();

  /// <summary>
  /// Get or create a pipeline by name.
  /// If the pipeline doesn't exist, it will be created using the registered
  /// shader paths.
  /// </summary>
  Vivid::VividPipeline *GetPipeline(const std::string &name);

  /// <summary>
  /// Register a pipeline with its shader paths.
  /// Must be called before GetPipeline for that name.
  /// </summary>
  void RegisterPipeline(
      const std::string &name, const std::string &vertShaderPath,
      const std::string &fragShaderPath,
      const Vivid::BlendConfig &blendConfig = Vivid::BlendConfig{},
      Vivid::PipelineType pipelineType = Vivid::PipelineType::Sprite2D);

  /// <summary>
  /// Check if a pipeline is registered.
  /// </summary>
  bool HasPipeline(const std::string &name) const;

  /// <summary>
  /// Get all registered pipeline names.
  /// </summary>
  std::vector<std::string> GetPipelineNames() const;

  /// <summary>
  /// Set terrain-specific descriptor layouts (16 textures for layered terrain).
  /// </summary>
  void SetTerrainLayouts(const std::vector<VkDescriptorSetLayout> &layouts);

private:
  RenderingPipelines() = default;
  ~RenderingPipelines();

  struct PipelineInfo {
    std::string vertShaderPath;
    std::string fragShaderPath;
    Vivid::BlendConfig blendConfig;
    Vivid::PipelineType pipelineType;
    std::unique_ptr<Vivid::VividPipeline> pipeline;
    bool useTerrainLayout =
        false; // Use terrain descriptor layout (16 textures)
  };

  Vivid::VividDevice *m_Device = nullptr;
  VkRenderPass m_RenderPass = VK_NULL_HANDLE;

  std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
  std::vector<VkDescriptorSetLayout> m_TerrainDescriptorSetLayouts;
  bool m_Initialized = false;

  std::unordered_map<std::string, PipelineInfo> m_Pipelines;
};

} // namespace Quantum
