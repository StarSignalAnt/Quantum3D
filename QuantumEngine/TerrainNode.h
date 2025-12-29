#pragma once
#include "GraphNode.h"
#include "TerrainLayer.h"
#include "VividDevice.h"
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>

namespace Quantum {

/// <summary>
/// A terrain node that renders a layered terrain surface.
/// The terrain mesh is centered at its origin (0,0,0 is the center).
/// Supports multiple texture layers blended via layer maps.
/// </summary>
class TerrainNode : public GraphNode {
public:
  /// <summary>
  /// Create a terrain node with specified dimensions.
  /// </summary>
  /// <param name="name">Node name</param>
  /// <param name="width">Terrain width (X axis)</param>
  /// <param name="depth">Terrain depth (Z axis)</param>
  /// <param name="divisions">Grid subdivisions</param>
  /// <param name="layerCount">Number of texture layers (1-4)</param>
  TerrainNode(const std::string &name = "Terrain", float width = 100.0f,
              float depth = 100.0f, int divisions = 100, int layerCount = 4);

  virtual ~TerrainNode();

  /// <summary>
  /// Initialize rendering resources (mesh buffers, default textures).
  /// Must be called after construction with a valid device.
  /// </summary>
  void Initialize(Vivid::VividDevice *device);

  // Getters
  float GetWidth() const { return m_Width; }
  float GetDepth() const { return m_Depth; }
  int GetDivisions() const { return m_Divisions; }
  int GetLayerCount() const { return m_LayerCount; }

  // Layer access
  TerrainLayer &GetLayer(int index);
  const TerrainLayer &GetLayer(int index) const;

  /// <summary>
  /// Set a layer texture at runtime by file path.
  /// </summary>
  /// <param name="layerIndex">Layer index (0-3)</param>
  /// <param name="type">"color", "normal", or "specular"</param>
  /// <param name="path">File path to texture</param>
  void SetLayerTexture(int layerIndex, const std::string &type,
                       const std::string &path);

  /// <summary>
  /// Check if textures need descriptor update.
  /// </summary>
  bool NeedsDescriptorUpdate() const { return m_DescriptorDirty; }
  void ClearDescriptorDirty() { m_DescriptorDirty = false; }

  // Clone support
  virtual std::shared_ptr<GraphNode> Clone() override;

  // Descriptor set for terrain layer textures
  VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
  void SetDescriptorSet(VkDescriptorSet set) { m_DescriptorSet = set; }

  /// <summary>
  /// Process any pending texture updates. Must be called on the render thread.
  /// </summary>
  void ProcessPendingUpdates();

  void Paint(const glm::vec3 &hitPoint, int layerIndex, float radius,
             float strength);
  void Sculpt(const glm::vec3 &hitPoint, float radius, float strength);
  void OnUpdate(float dt) override; // Called each frame

private:
  struct PendingTextureUpdate {
    int layer;
    std::string type;
    std::string path;
  };
  /// <summary>
  /// Generate the terrain mesh grid.
  /// Origin (0,0,0) is at the center of the terrain.
  /// </summary>
  void GenerateTerrainMesh();

  /// <summary>
  /// Create default textures for layers.
  /// </summary>
  void CreateDefaultTextures(Vivid::VividDevice *device);

  float m_Width;
  float m_Depth;
  int m_Divisions;
  int m_LayerCount;

  std::vector<TerrainLayer> m_Layers;

  // Vulkan descriptor set for terrain layer textures (16 samplers)
  VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

  // Flag indicating descriptor needs update after texture change
  bool m_DescriptorDirty = false;

  // Device pointer for texture creation
  Vivid::VividDevice *m_Device = nullptr;

  // Thread-safe queue for texture updates from the UI thread
  std::vector<PendingTextureUpdate> m_PendingUpdates;
  std::mutex m_UpdatesMutex;

  // Local CPU copy of layer blend maps (R channel only technically needed, but
  // likely stored as RGBA) Each vector corresponds to a layer (0-3)
  std::vector<std::vector<unsigned char>> m_LayerBlendData;
  std::vector<bool> m_LayerDirty; // Tracks if a layer needs GPU upload
  bool m_AnyLayerDirty = false;   // Optimization: Quick check for updates
  const int m_BlendMapSize = 512; // Resolution of blend maps

  void InitializeBlendMaps();
  void UpdateGPUTextures();
};

} // namespace Quantum
