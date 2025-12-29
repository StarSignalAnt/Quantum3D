#pragma once
#include "CLBase.h"
#include "glm/glm.hpp"
#include <vector>

namespace Quantum {

/// GPU-accelerated lightmap baking using OpenCL
class CLLightmapper : public CLBase {
public:
  CLLightmapper();

  // Light data structure matching OpenCL kernel (16-byte aligned)
  struct LightData {
    glm::vec4 positionAndRange; // xyz=pos, w=range
    glm::vec4 colorAndType;     // xyz=color, w=type (cast to float)
    glm::vec4 direction;        // xyz=dir, w=unused
  };

  // Texel data structure matching OpenCL kernel (16-byte aligned)
  struct TexelData {
    glm::vec4 worldPos; // w=unused
    glm::vec4 normal;   // w=unused
    int valid;
    int padding[3]; // Align to 48 bytes (16+16+16)
  };

  /// Bake lightmap for all texels using GPU acceleration
  /// @param texels Array of texel world positions and normals
  /// @param numTexels Number of texels (resolution * resolution)
  /// @param lights Array of light data
  /// @param numLights Number of lights in scene
  /// @param sceneTriangles Flattened scene triangles (9 floats per triangle:
  /// v0, v1, v2)
  /// @param numTriangles Number of triangles in scene
  /// @param enableShadows Whether to trace shadow rays
  /// @param outLighting Output lighting buffer (3 floats per texel: RGB)
  /// @return true on success
  bool BakeLightmap(const std::vector<TexelData> &texels,
                    const std::vector<LightData> &lights,
                    const std::vector<float> &sceneTriangles, int numTriangles,
                    bool enableShadows, std::vector<glm::vec3> &outLighting);

  /// Bake indirect lighting (GI) using GPU acceleration (Single Bounce)
  /// @param texels Array of texel world positions and normals
  /// @param lights Array of light data
  /// @param sceneTriangles Flattened scene triangles (9 floats per triangle:
  /// v0, v1, v2)
  /// @param numTriangles Number of triangles in scene
  /// @param enableShadows Whether to trace shadow rays
  /// @param samples Number of samples per texel for indirect lighting
  /// @param intensity Intensity multiplier for indirect lighting
  /// @param outIndirect Output indirect lighting buffer (3 floats per texel:
  /// RGB)
  /// @return true on success
  bool BakeIndirect(const std::vector<TexelData> &texels,
                    const std::vector<LightData> &lights,
                    const std::vector<float> &sceneTriangles, int numTriangles,
                    bool enableShadows, int samples, float intensity,
                    std::vector<glm::vec3> &outIndirect);

  /// Check if OpenCL initialization succeeded
  bool IsValid() const { return m_Initialized; }

private:
  bool m_Initialized = false;

  // Cached GPU buffers
  cl::Buffer m_TexelBuffer;
  cl::Buffer m_LightBuffer;
  cl::Buffer m_TriangleBuffer;
  cl::Buffer m_OutputBuffer;

  // Cached sizes to avoid reallocating
  size_t m_CachedTexelCount = 0;
  size_t m_CachedLightCount = 0;
  size_t m_CachedTriangleCount = 0;

  bool CheckCLError(cl_int err, const char *operation) const;
};

} // namespace Quantum
