#pragma once
#include "PointShadowMap.h"
#include "SceneGraph.h"
#include "ShadowPipeline.h"
#include "Texture2D.h"
#include "VividBuffer.h"
#include "VividDevice.h"
#include "VividPipeline.h"
#include "VividRenderer.h"
#include <array>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Vivid {
class Draw2D;
}

namespace Quantum {

// Forward declarations
class Material;

/// <summary>
/// Handles rendering of a SceneGraph using Vulkan.
/// Manages descriptor sets, uniform buffers, and scene traversal.
/// </summary>
class SceneRenderer {
public:
  SceneRenderer(Vivid::VividDevice *device, Vivid::VividRenderer *renderer);
  ~SceneRenderer();

  // Initialize rendering resources
  void Initialize();

  // Cleanup rendering resources
  void Shutdown();

  // Set the scene graph to render
  void SetSceneGraph(std::shared_ptr<SceneGraph> sceneGraph);

  // Get the current scene graph
  std::shared_ptr<SceneGraph> GetSceneGraph() const { return m_SceneGraph; }

  // Render the scene graph (includes shadow pass if enabled)
  void RenderScene(VkCommandBuffer cmd, int width, int height);

  // Get the descriptor set layout (needed for pipeline creation)
  VkDescriptorSetLayout GetDescriptorSetLayout() const {
    return m_DescriptorSetLayout;
  }

  // Refresh material textures (called after model import)
  void RefreshMaterialTextures();

  // Shadow control
  bool IsShadowsEnabled() const { return m_ShadowsEnabled; }
  void SetShadowsEnabled(bool enabled) { m_ShadowsEnabled = enabled; }
  PointShadowMap *GetShadowMap() const { return m_ShadowMap.get(); }

  // Render shadow depth pass (call BEFORE BeginRenderPass for main scene)
  void RenderShadowPass(VkCommandBuffer cmd);

  // Debug rendering of shadow map faces
  void RenderShadowDebug(Vivid::Draw2D *draw2d);

private:
  void CreateDescriptorSetLayout();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateUniformBuffer();
  void RenderNode(VkCommandBuffer cmd, GraphNode *node, int width, int height);

  // Shadow rendering
  void InitializeShadowResources();
  void RenderNodeToShadow(VkCommandBuffer cmd, GraphNode *node,
                          const glm::mat4 &lightSpaceMatrix);

  Vivid::VividDevice *m_Device = nullptr;
  Vivid::VividRenderer *m_Renderer = nullptr;

  // Scene
  std::shared_ptr<SceneGraph> m_SceneGraph;

  // Vulkan resources
  VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

  // Per-frame UBO buffers to prevent race conditions
  // CPU writes to frame N's buffer while GPU reads from frame N-1's buffer
  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
  std::array<std::unique_ptr<Vivid::VividBuffer>, MAX_FRAMES_IN_FLIGHT>
      m_UniformBuffers;
  int m_CurrentFrameIndex = 0; // Track which frame buffer to use

  // Dynamic uniform buffer for multi-light support
  size_t m_MaxDraws =
      65536; // Initial max draw calls per frame (can grow dynamicall)
  void ResizeUniformBuffers(size_t requiredDraws);
  uint32_t m_UniformBufferAlignment =
      256; // Typical minUniformBufferOffsetAlignment
  uint32_t m_AlignedUBOSize = 0;
  mutable size_t m_CurrentDrawIndex = 0;

  bool m_Initialized = false;

  // Animation state
  float m_AnimationAngle = 0.0f;

  // Debug counters (reset each frame)
  mutable int m_RenderNodeCount = 0;
  mutable int m_RenderMeshCount = 0;

  // Current pipeline tracking (for per-material pipeline switching)
  mutable Vivid::VividPipeline *m_CurrentPipeline = nullptr;

  // Current texture tracking (for per-mesh texture binding)
  mutable Vivid::Texture2D *m_CurrentTexture = nullptr;

  // Default white texture for meshes without textures
  std::shared_ptr<Vivid::Texture2D> m_DefaultTexture;

  std::unique_ptr<PointShadowMap> m_ShadowMap;
  std::unique_ptr<ShadowPipeline> m_ShadowPipeline;
  bool m_ShadowsEnabled = true;

  // Multi-light rendering state
  mutable size_t m_CurrentLightIndex = 0;

  // Debug textures for shadow faces
  std::vector<std::unique_ptr<Vivid::Texture2D>> m_FaceTextures;

  void UpdateTextureDescriptor(Vivid::Texture2D *texture);
  void UpdatePBRTextures(Material *material);
  void UpdateFirstMaterialTextures(GraphNode *node);
  void CreateMaterialDescriptorSetsRecursive(GraphNode *node);
};
} // namespace Quantum
