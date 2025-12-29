#pragma once
#include "Intersections.h"
#include "Mesh3D.h"
#include "VividDevice.h"
#include "glm/glm.hpp"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>


class Intersections;

namespace Quantum {

class SceneRenderer;
class TerrainNode;

class TerrainGizmo {
public:
  TerrainGizmo(Vivid::VividDevice *device);
  ~TerrainGizmo();

  void Initialize();
  void Render(SceneRenderer *renderer, VkCommandBuffer cmd,
              const glm::mat4 &view, const glm::mat4 &proj);

  void SetPosition(const glm::vec3 &position);
  void SetSize(float size);
  glm::vec3 GetPosition() const { return m_Position; }

  /// Update vertex heights to conform to terrain surface
  void UpdateToTerrain(TerrainNode *terrain);

  /// Raycast against terrain mesh (used for mouse picking)
  CastResult RaycastTerrain(TerrainNode *terrain, const glm::vec3 &rayOrigin,
                            const glm::vec3 &rayDir);

private:
  void RebuildMesh();

  Vivid::VividDevice *m_Device;
  std::shared_ptr<Mesh3D> m_Mesh;
  glm::vec3 m_Position = glm::vec3(0.0f);
  float m_Scale = 1.0f;

  // Color: Light Blue (0, 1, 1)
  glm::vec4 m_Color = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);

  // For dynamic updates
  bool m_NeedsTerrainUpdate = true;

  // GPU-accelerated intersection testing
  std::unique_ptr<Intersections> m_Intersections;

  // Original local vertex positions (so we can recalculate world positions
  // correctly)
  std::vector<glm::vec2> m_OriginalLocalXZ;
};

} // namespace Quantum
