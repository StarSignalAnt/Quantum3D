#pragma once
#include "CLBase.h"
#include "Mesh3D.h"
#include "glm/glm.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Quantum {
class Mesh3D;
}

struct CastResult {
  float Distance = 0.0;
  bool Hit = false;
  int MeshIndex = -1;
  glm::vec3 HitPoint = {0.0f, 0.0f, 0.0f};
};

class Intersections : public CLBase {
public:
  Intersections();

  /// <summary>
  /// Cast a ray against a Mesh3D in local space.
  /// </summary>
  /// <param name="pos">Ray origin in local/model space</param>
  /// <param name="dir">Ray direction (normalized)</param>
  /// <param name="mesh">The mesh to test against</param>
  /// <returns>CastResult with hit info</returns>
  CastResult CastMesh(glm::vec3 pos, glm::vec3 dir,
                      const Quantum::Mesh3D *mesh);

  /// <summary>
  /// Cast a ray against a Mesh3D with a model matrix (world-space ray).
  /// The ray is transformed to local space before intersection.
  /// </summary>
  /// <param name="modelMatrix">Model-to-world transformation matrix</param>
  /// <param name="pos">Ray origin in world space</param>
  /// <param name="dir">Ray direction in world space (normalized)</param>
  /// <param name="mesh">The mesh to test against</param>
  /// <returns>CastResult with hit info in world space</returns>
  CastResult CastMesh(const glm::mat4 &modelMatrix, glm::vec3 pos,
                      glm::vec3 dir, const Quantum::Mesh3D *mesh);

  /// <summary>
  /// Invalidate cached geometry buffer for a mesh.
  /// Call this when mesh geometry changes.
  /// </summary>
  void InvalidateMesh(const Quantum::Mesh3D *mesh);

  /// <summary>
  /// Clear all cached geometry buffers.
  /// </summary>
  void ClearCache();

private:
  // Buffers for ray casting
  struct CastBuffers {
    cl::Buffer posBuffer;
    cl::Buffer dirBuffer;
    cl::Buffer resultBuffer;
    cl::Buffer hitPointBuffer;
    bool initialized = false;
  };

  CastBuffers m_CastBuffers;

  // Thread safety
  std::mutex m_CastMutex;

  // Cached mesh geometry with version tracking for dirty detection
  struct MeshCacheEntry {
    cl::Buffer triBuffer;           // GPU buffer with triangle vertex data
    std::vector<glm::vec3> triData; // CPU-side triangle data cache
    uint64_t geometryVersion = 0;   // Cached version for dirty check
  };

  // Cached geometry keyed by mesh pointer
  std::unordered_map<const Quantum::Mesh3D *, MeshCacheEntry> m_MeshCache;

  // Helper methods
  void InitializeBuffers();
  size_t GetOptimalWorkGroupSize(size_t numTris) const;
  size_t RoundUpToMultiple(size_t value, size_t multiple) const;

  // Error handling helper
  bool CheckCLError(cl_int err, const char *operation) const;
};