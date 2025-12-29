#pragma once
#include "CLLightmapper.h"
#include "Intersections.h"
#include "LightNode.h"
#include "LightmapUVGenerator.h"
#include "Mesh3D.h"
#include "SceneGraph.h"
#include "Texture2D.h"
#include "VividDevice.h"
#include "glm/glm.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace Quantum {

/// <summary>
/// Settings for lightmap baking.
/// </summary>
struct BakeSettings {
  int resolution = 256;     // Per-mesh lightmap resolution
  int shadowSamples = 16;   // Shadow ray samples for soft shadows
  int giBounces = 3;        // Number of light bounces for GI
  int giSamples = 64;       // Hemisphere samples per texel for GI
  float giIntensity = 1.0f; // GI contribution multiplier
  bool enableShadows = true;
  bool enableGI = true;
  bool useGPU = true; // Use OpenCL GPU acceleration when available
};

/// <summary>
/// A texel in the lightmap with its world-space data.
/// </summary>
struct LightmapTexel {
  glm::vec3 worldPos;
  glm::vec3 worldNormal;
  bool valid = false; // Is this texel part of the mesh?
  int triangleIndex = -1;
  glm::vec2 barycentrics;
};

/// <summary>
/// Result of baking a single mesh.
/// </summary>
struct BakedLightmap {
  std::vector<glm::vec3> pixels; // RGB lighting data
  int width = 0;
  int height = 0;
  std::string meshName;
};

/// <summary>
/// Bakes static lightmaps for meshes in a scene.
/// Supports point lights with shadows and global illumination.
/// Uses GPU acceleration via OpenCL when available.
/// </summary>
class LightmapBaker {
public:
  using ProgressCallback =
      std::function<void(float progress, const std::string &status)>;

  LightmapBaker();
  ~LightmapBaker() = default;

  /// <summary>
  /// Bake lightmaps for all meshes in the scene.
  /// </summary>
  /// <param name="device">Vulkan device for texture creation</param>
  /// <param name="sceneGraph">Scene to bake</param>
  /// <param name="settings">Bake settings</param>
  /// <param name="callback">Optional progress callback</param>
  /// <returns>True if successful</returns>
  bool Bake(Vivid::VividDevice *device, std::shared_ptr<SceneGraph> sceneGraph,
            const BakeSettings &settings = BakeSettings(),
            ProgressCallback callback = nullptr);

  /// <summary>
  /// Get the last error message.
  /// </summary>
  const std::string &GetLastError() const { return m_LastError; }

  /// <summary>
  /// Get baked lightmaps (available after successful bake).
  /// </summary>
  const std::vector<BakedLightmap> &GetBakedLightmaps() const {
    return m_BakedLightmaps;
  }

  /// Check if GPU acceleration is available
  bool IsGPUAvailable() const { return m_CLLightmapper.IsValid(); }

private:
  // Step 1: Generate UV2 for a mesh if needed
  bool EnsureUV2(Vivid::VividDevice *device, Mesh3D *mesh, int resolution);

  // Step 2: Rasterize triangles to generate texel data
  void RasterizeMesh(const Mesh3D *mesh, const glm::mat4 &worldMatrix,
                     int resolution, std::vector<LightmapTexel> &texels);

  // Step 3: Compute direct lighting for each texel (CPU fallback)
  void ComputeDirectLighting(const std::vector<LightmapTexel> &texels,
                             const std::vector<LightNode *> &lights,
                             std::vector<glm::vec3> &lighting,
                             const BakeSettings &settings);

  // Step 3b: Compute direct lighting using GPU (OpenCL)
  bool ComputeDirectLightingGPU(const std::vector<LightmapTexel> &texels,
                                const std::vector<LightNode *> &lights,
                                std::vector<glm::vec3> &lighting,
                                const BakeSettings &settings);

  // Step 4: Trace shadow rays (uses Intersections class) - CPU fallback
  float TraceShadowRay(const glm::vec3 &texelPos, const glm::vec3 &lightPos);

  // Step  // Compute global illumination (CPU)
  void ComputeGlobalIllumination(std::vector<LightmapTexel> &texels,
                                 const std::vector<LightNode *> &lights,
                                 std::vector<glm::vec3> &lighting,
                                 const BakeSettings &settings);

  // Compute global illumination (GPU)
  bool ComputeGlobalIlluminationGPU(const std::vector<LightmapTexel> &texels,
                                    const std::vector<LightNode *> &lights,
                                    std::vector<glm::vec3> &lighting,
                                    const BakeSettings &settings);

  // Create GPU texture from baked data
  std::shared_ptr<Vivid::Texture2D>
  CreateLightmapTexture(Vivid::VividDevice *device, const BakedLightmap &baked);

  // Collect all light nodes from scene
  std::vector<LightNode *>
  CollectLights(std::shared_ptr<SceneGraph> sceneGraph);

  // Collect all mesh nodes from scene (with world matrices)
  struct MeshInstance {
    Mesh3D *mesh;
    glm::mat4 worldMatrix;
    GraphNode *node;
  };
  std::vector<MeshInstance>
  CollectMeshes(std::shared_ptr<SceneGraph> sceneGraph);

  // Sample hemisphere for GI
  glm::vec3 SampleHemisphere(const glm::vec3 &normal, float u1, float u2);

  // Collect all scene triangles in world space for GPU shadow tracing
  void CollectSceneTriangles(std::vector<float> &triangles, int &numTriangles);

  std::string m_LastError;
  std::vector<BakedLightmap> m_BakedLightmaps;
  LightmapUVGenerator m_UVGenerator;
  Intersections m_Intersections;
  CLLightmapper m_CLLightmapper; // GPU lightmapper

  // All mesh instances for shadow tracing
  std::vector<MeshInstance> m_AllMeshes;
};

} // namespace Quantum
