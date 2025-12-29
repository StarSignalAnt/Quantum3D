#pragma once
#include "Mesh3D.h"
#include <functional>

namespace Quantum {

/// <summary>
/// Generates lightmap UV coordinates (UV2) for meshes using xatlas library.
/// </summary>
class LightmapUVGenerator {
public:
  /// Settings for UV generation
  struct Settings {
    float maxChartArea = 0.0f;      // 0 = unlimited
    float maxBoundaryLength = 0.0f; // 0 = unlimited
    float normalDeviationWeight = 2.0f;
    float roundnessWeight = 0.01f;
    float straightnessWeight = 6.0f;
    float normalSeamWeight = 4.0f;
    float textureSeamWeight = 0.5f;
    int padding = 2; // Pixel padding between charts
  };

  /// Progress callback (0.0 to 1.0)
  using ProgressCallback = std::function<void(float progress)>;

  LightmapUVGenerator() = default;
  ~LightmapUVGenerator() = default;

  /// <summary>
  /// Generate UV2 coordinates for a mesh using xatlas.
  /// This will modify the mesh vertices to add lightmap UVs and
  /// potentially add new vertices at seams.
  /// </summary>
  /// <param name="mesh">The mesh to generate UVs for</param>
  /// <param name="resolution">Target lightmap resolution (affects UV
  /// scale)</param> <param name="settings">UV generation settings</param>
  /// <param name="callback">Optional progress callback</param>
  /// <returns>True if successful, false on error</returns>
  bool GenerateUV2(Mesh3D *mesh, int resolution,
                   const Settings &settings = Settings(),
                   ProgressCallback callback = nullptr);

  /// <summary>
  /// Get the last error message if GenerateUV2 failed.
  /// </summary>
  const std::string &GetLastError() const { return m_LastError; }

private:
  std::string m_LastError;
};

} // namespace Quantum
