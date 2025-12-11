#pragma once
#include "VividDevice.h"
#include "VividPipeline.h"
#include <memory>
#include <string>
#include <unordered_map>

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
  void Initialize(Vivid::VividDevice *device, VkRenderPass renderPass,
                  VkDescriptorSetLayout descriptorSetLayout);

  /// <summary>
  /// Shutdown and cleanup all pipelines.
  /// </summary>
  void Shutdown();

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

private:
  RenderingPipelines() = default;
  ~RenderingPipelines();

  struct PipelineInfo {
    std::string vertShaderPath;
    std::string fragShaderPath;
    Vivid::BlendConfig blendConfig;
    Vivid::PipelineType pipelineType;
    std::unique_ptr<Vivid::VividPipeline> pipeline;
  };

  Vivid::VividDevice *m_Device = nullptr;
  VkRenderPass m_RenderPass = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
  bool m_Initialized = false;

  std::unordered_map<std::string, PipelineInfo> m_Pipelines;
};

} // namespace Quantum
