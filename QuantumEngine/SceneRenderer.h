#pragma once
#include "GizmoBase.h"
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
  // Get the descriptor set layout (returns Material layout for Materials)
  VkDescriptorSetLayout GetDescriptorSetLayout() const {
    return m_MaterialSetLayout;
  }
  VkDescriptorSetLayout GetGlobalSetLayout() const { return m_GlobalSetLayout; }

  // Refresh material textures (called after model import)
  void RefreshMaterialTextures();

  // Shadow control
  bool IsShadowsEnabled() const { return m_ShadowsEnabled; }
  void SetShadowsEnabled(bool enabled) { m_ShadowsEnabled = enabled; }
  PointShadowMap *GetShadowMap(uint32_t index) const {
    if (index < m_ShadowMaps.size())
      return m_ShadowMaps[index].get();
    return nullptr;
  }
  size_t GetShadowMapCount() const { return m_ShadowMaps.size(); }

  // Render shadow depth pass (call BEFORE BeginRenderPass for main scene)
  void RenderShadowPass(VkCommandBuffer cmd);

  // Debug rendering of shadow map faces
  void RenderShadowDebug(Vivid::Draw2D *draw2d);

  // Selection Helpers
  void RegisterWireframePipeline();
  void RenderSelection(VkCommandBuffer cmd,
                       std::shared_ptr<GraphNode> selectedNode);
  // Gizmo Support
  void DrawGizmoMesh(VkCommandBuffer cmd, std::shared_ptr<Mesh3D> mesh,
                     const glm::mat4 &model, const glm::vec3 &color,
                     const glm::mat4 &view, const glm::mat4 &proj);
  void SetGizmoPosition(const glm::vec3 &position);
  void SetGizmoTargetNode(std::shared_ptr<GraphNode> node);
  void SetGizmoViewState(const glm::mat4 &view, const glm::mat4 &proj, int w,
                         int h);
  bool OnGizmoMouseClicked(int x, int y, bool isPressed, int width, int height);
  void OnGizmoMouseMoved(int x, int y);
  bool IsGizmoDragging() const;
  void SetGizmoSpace(GizmoSpace space);
  void SetGizmoType(GizmoType type);

private:
  // Gizmo instances (all types stored, one active at a time)
  std::unique_ptr<class GizmoBase> m_TranslateGizmo;
  std::unique_ptr<class GizmoBase> m_RotateGizmo;
  GizmoBase *m_ActiveGizmo = nullptr; // Points to active gizmo (not owning)
  void CreateDescriptorSetLayout();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateUniformBuffer();
  void RenderNode(VkCommandBuffer cmd, GraphNode *node, int width, int height);

  // Shadow rendering
  void InitializeShadowResources();
  void RenderNodeToShadow(VkCommandBuffer cmd, GraphNode *node,
                          const glm::mat4 &lightSpaceMatrix,
                          const glm::vec3 &lightPos, float farPlane);

  Vivid::VividDevice *m_Device = nullptr;
  Vivid::VividRenderer *m_Renderer = nullptr;

  // Scene
  std::shared_ptr<SceneGraph> m_SceneGraph;

  // Vulkan resources
  // Vulkan resources
  VkDescriptorSetLayout m_GlobalSetLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_MaterialSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet>
      m_GlobalDescriptorSets; // [Frame * Lights + Light]
  VkDescriptorSet m_DefaultMaterialSet = VK_NULL_HANDLE;

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
  uint32_t m_UniformBufferAlignment = 256;
  uint32_t m_AlignedUBOSize = 0;
  mutable size_t m_CurrentDrawIndex = 0;

  // Dedicated Gizmo UBO (separate from scene to prevent cross-frame corruption)
  std::unique_ptr<Vivid::VividBuffer> m_GizmoUniformBuffer;
  VkDescriptorSet m_GizmoDescriptorSet = VK_NULL_HANDLE;
  mutable size_t m_GizmoDrawIndex = 0;

  // Track UBO capacity
  size_t m_CurrentUBOCapacity = 0;

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

  std::vector<std::unique_ptr<PointShadowMap>> m_ShadowMaps;
  std::unique_ptr<ShadowPipeline> m_ShadowPipeline;
  bool m_ShadowsEnabled = true;

  // Multi-light rendering state
  mutable size_t m_CurrentLightIndex = 0;

  // Debug textures for shadow faces
  std::vector<std::unique_ptr<Vivid::Texture2D>> m_FaceTextures;

  // Selection Helpers
  std::shared_ptr<Mesh3D> m_UnitCube;

  void UpdateTextureDescriptor(Vivid::Texture2D *texture);
  void UpdatePBRTextures(Material *material);
  void UpdateFirstMaterialTextures(GraphNode *node);
  void CreateMaterialDescriptorSetsRecursive(GraphNode *node);
};
} // namespace Quantum
