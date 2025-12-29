#pragma once
#include "LightmapBaker.h"
#include "glm/glm.hpp"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace Quantum {

/// <summary>
/// Header for Quantum Lightmap (.qlm) binary files.
/// </summary>
#pragma pack(push, 1)
struct QLightmapHeader {
  char magic[4] = {'Q', 'L', 'M', '1'}; // File signature
  uint32_t version = 1;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t format = 0;         // 0 = RGB float (vec3), 1 = RGBA8
  uint32_t meshNameLength = 0; // Length of mesh name string
  // Followed by: meshName (UTF-8), then pixel data
};
#pragma pack(pop)

/// <summary>
/// Handles reading and writing of Quantum Lightmap (.qlm) files.
/// </summary>
class LightmapFile {
public:
  /// <summary>
  /// Save a baked lightmap to a .qlm file.
  /// </summary>
  /// <param name="path">Output file path</param>
  /// <param name="lightmap">Baked lightmap data</param>
  /// <returns>True if successful</returns>
  static bool Save(const std::string &path, const BakedLightmap &lightmap);

  /// <summary>
  /// Load a baked lightmap from a .qlm file.
  /// </summary>
  /// <param name="path">Input file path</param>
  /// <param name="lightmap">Output lightmap data</param>
  /// <returns>True if successful</returns>
  static bool Load(const std::string &path, BakedLightmap &lightmap);

  /// <summary>
  /// Check if a file is a valid .qlm file.
  /// </summary>
  static bool IsValidFile(const std::string &path);

  /// <summary>
  /// Get the last error message.
  /// </summary>
  static const std::string &GetLastError();

private:
  static std::string s_LastError;
};

} // namespace Quantum
