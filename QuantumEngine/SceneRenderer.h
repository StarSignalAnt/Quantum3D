#pragma once
#include "DirectionalShadowMap.h"
#include "GizmoBase.h"
#include "Intersections.h"
#include "LightmapBaker.h"
#include "PointShadowMap.h"
#include "SceneGraph.h"
#include "ShadowPipeline.h"
#include "TerrainGizmo.h"
#include "TerrainNode.h"
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
  void RenderScene(VkCommandBuffer cmd, int width, int height,
                   float time); // Added time

  // Render water reflection/refraction passes (call before main pass)
  void RenderWaterPasses(VkCommandBuffer cmd, float time);

  // Get the descriptor set layout (needed for pipeline creation)
  // Get the descriptor set layout (returns Material layout for Materials)
  VkDescriptorSetLayout GetDescriptorSetLayout() const {
    return m_MaterialSetLayout;
  }
  VkDescriptorSetLayout GetGlobalSetLayout() const { return m_GlobalSetLayout; }
  VkDescriptorSetLayout GetTerrainSetLayout() const {
    return m_TerrainSetLayout;
  }

  // Refresh material textures (called after model import)
  void RefreshMaterialTextures();

  // Check for dirty terrain nodes and refresh their descriptors
  void CheckAndRefreshDirtyTerrains(GraphNode *node);

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

  // Invalidate texture descriptors (call before Draw2D destruction on swapchain
  // recreation)
  void InvalidateTextureDescriptors();

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
  void SetShowTerrainGizmo(bool show) { m_ShowTerrainGizmo = show; }
  void SetTerrainGizmoSize(float size);
  glm::vec3 GetTerrainGizmoPosition() const {
    if (m_TerrainGizmo)
      return m_TerrainGizmo->GetPosition();
    return glm::vec3(0.0f);
  }
  void SetTerrainGizmoPosition(const glm::vec3 &position) {
    if (m_TerrainGizmo) {
      m_TerrainGizmo->SetPosition(position);
      // Update gizmo to conform to terrain
      if (m_SceneGraph) {
        auto terrain = m_SceneGraph->GetTerrainNode();
        if (terrain) {
          m_TerrainGizmo->UpdateToTerrain(
              dynamic_cast<TerrainNode *>(terrain.get()));
        }
      }
    }
  }

  /// Update terrain gizmo to conform to terrain (call after sculpting)
  void UpdateTerrainGizmo() {
    if (m_TerrainGizmo && m_SceneGraph) {
      auto terrain = m_SceneGraph->GetTerrainNode();
      if (terrain) {
        m_TerrainGizmo->UpdateToTerrain(
            dynamic_cast<TerrainNode *>(terrain.get()));
      }
    }
  }

  /// Raycast against the terrain mesh
  /// Returns CastResult with Hit=true if terrain was hit
  CastResult RaycastTerrain(const glm::vec3 &rayOrigin,
                            const glm::vec3 &rayDir) {
    if (!m_TerrainGizmo || !m_SceneGraph) {
      return CastResult{false};
    }

    auto terrain = m_SceneGraph->GetTerrainNode();
    if (!terrain) {
      return CastResult{false};
    }

    auto *terrainNode = dynamic_cast<TerrainNode *>(terrain.get());
    if (!terrainNode) {
      return CastResult{false};
    }

    const auto &meshes = terrainNode->GetMeshes();
    if (meshes.empty() || !meshes[0]) {
      return CastResult{false};
    }

    // Use TerrainGizmo's Intersections instance for raycasting
    return m_TerrainGizmo->RaycastTerrain(terrainNode, rayOrigin, rayDir);
  }

  // ========== Lightmap Baking ==========

  /// Bake lightmaps for all meshes in the current scene
  /// Returns true if baking was successful
  bool BakeLightmaps(const BakeSettings &settings = BakeSettings(),
                     LightmapBaker::ProgressCallback callback = nullptr) {
    if (!m_SceneGraph || !m_Device) {
      return false;
    }
    return m_LightmapBaker.Bake(m_Device, m_SceneGraph, settings, callback);
  }

  /// Check if lightmaps have been baked
  bool HasBakedLightmaps() const {
    return !m_LightmapBaker.GetBakedLightmaps().empty();
  }

  /// Get the lightmap baker for advanced access
  LightmapBaker &GetLightmapBaker() { return m_LightmapBaker; }
  const LightmapBaker &GetLightmapBaker() const { return m_LightmapBaker; }

private:
  // Lightmap baker instance
  LightmapBaker m_LightmapBaker;
  // Gizmo instances (all types stored, one active at a time)
  std::unique_ptr<class TranslateGizmo> m_TranslateGizmo;
  std::unique_ptr<class RotateGizmo> m_RotateGizmo;
  std::unique_ptr<class TerrainGizmo> m_TerrainGizmo;
  GizmoBase *m_ActiveGizmo = nullptr; // Points to active gizmo (not owning)
  bool m_ShowTerrainGizmo = false;
  void CreateDescriptorSetLayout();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateUniformBuffer();
  void RenderNode(VkCommandBuffer cmd, GraphNode *node, int width, int height);
  void RenderNode(VkCommandBuffer cmd, GraphNode *node, int width, int height,
                  const glm::mat4 &view, const glm::mat4 &proj,
                  bool skipWater = false); // Overload for custom view

  // Shadow rendering
  void InitializeShadowResources();
  void RenderNodeToShadow(VkCommandBuffer cmd, GraphNode *node,
                          const glm::mat4 &lightSpaceMatrix,
                          const glm::vec4 &lightInfo);

  Vivid::VividDevice *m_Device = nullptr;
  Vivid::VividRenderer *m_Renderer = nullptr;

  // Scene
  std::shared_ptr<SceneGraph> m_SceneGraph;

  // Vulkan resources
  // Vulkan resources
  VkDescriptorSetLayout m_GlobalSetLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_MaterialSetLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_TerrainSetLayout =
      VK_NULL_HANDLE; // 16 bindings for terrain layers
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

  // Viewport aspect ratio (for matching reflection camera projection)
  float m_ViewportAspect = 1.0f;

  // Clip plane direction for water passes
  // 1.0 = reflection (clip below Y=0), -1.0 = refraction (clip above Y=0), 0.0
  // = no clip
  float m_ClipPlaneDir = 0.0f;

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
  std::vector<std::unique_ptr<DirectionalShadowMap>> m_DirShadowMaps;
  std::unique_ptr<PointShadowMap> m_NullShadowMap;
  std::unique_ptr<DirectionalShadowMap> m_NullDirShadowMap;
  std::unique_ptr<ShadowPipeline> m_ShadowPipeline;
  bool m_ShadowsEnabled = true;

  // Multi-light rendering state
  mutable size_t m_CurrentLightIndex = 0;

  // Scene center for shadow map targeting (computed from camera)
  mutable glm::vec3 m_SceneCenter = glm::vec3(0.0f);

  // Cached directional shadow matrix to ensure consistency between passes
  mutable glm::mat4 m_CachedDirShadowMatrix = glm::mat4(1.0f);

  // Debug textures for shadow faces
  std::vector<std::unique_ptr<Vivid::Texture2D>> m_FaceTextures;
  // Debug textures for directional shadow maps
  std::vector<std::unique_ptr<Vivid::Texture2D>> m_DirShadowDebugTextures;

  // Selection Helpers
  std::shared_ptr<Mesh3D> m_UnitCube;

  void UpdateTextureDescriptor(Vivid::Texture2D *texture);
  void UpdatePBRTextures(Material *material);
  void UpdateFirstMaterialTextures(GraphNode *node);
  void CreateMaterialDescriptorSetsRecursive(GraphNode *node);

  // Water Rendering
  void CreateWaterResources();
  void DestroyWaterResources();
  void RenderReflection(VkCommandBuffer cmd,
                        float time); // Render reflected scene
  void RenderRefraction(VkCommandBuffer cmd,
                        float time); // Render refracted scene
  void AssignWaterTextures(GraphNode *node);

  bool m_WaterResourcesCreated = false;
  uint32_t m_WaterResolution = 1024; // Resolution for reflection/refraction
  VkRenderPass m_WaterRenderPass = VK_NULL_HANDLE;

  // Reflection
  VkImage m_ReflectionImage = VK_NULL_HANDLE;
  VkDeviceMemory m_ReflectionMemory = VK_NULL_HANDLE;
  VkImageView m_ReflectionImageView = VK_NULL_HANDLE;
  VkFramebuffer m_ReflectionFramebuffer = VK_NULL_HANDLE;
  std::shared_ptr<Vivid::Texture2D>
      m_ReflectionTexture; // For binding to shader

  // Refraction
  VkImage m_RefractionImage = VK_NULL_HANDLE;
  VkDeviceMemory m_RefractionMemory = VK_NULL_HANDLE;
  VkImageView m_RefractionImageView = VK_NULL_HANDLE;
  VkFramebuffer m_RefractionFramebuffer = VK_NULL_HANDLE;
  std::shared_ptr<Vivid::Texture2D>
      m_RefractionTexture; // For binding to shader

  // Separate depth buffers for each pass to avoid corruption
  VkImage m_ReflectionDepthImage = VK_NULL_HANDLE;
  VkDeviceMemory m_ReflectionDepthMemory = VK_NULL_HANDLE;
  VkImageView m_ReflectionDepthImageView = VK_NULL_HANDLE;

  VkImage m_RefractionDepthImage = VK_NULL_HANDLE;
  VkDeviceMemory m_RefractionDepthMemory = VK_NULL_HANDLE;
  VkImageView m_RefractionDepthImageView = VK_NULL_HANDLE;

  // Helper to create texture image
  void CreateAttachment(VkFormat format, VkImageUsageFlags usage,
                        VkImage &image, VkDeviceMemory &memory,
                        VkImageView &view);
};
} // namespace Quantum
