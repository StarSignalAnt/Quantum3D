#pragma once
#include <memory>
#include <string>

namespace Vivid {
class Texture2D;
}

namespace Quantum {

/// <summary>
/// Represents a single texture layer for terrain rendering.
/// Each layer has color, normal, and specular maps (tiled),
/// plus a layer map that defines blend strength across the terrain.
/// </summary>
struct TerrainLayer {
  // Texture maps (tiled across terrain based on tiling factor)
  std::shared_ptr<Vivid::Texture2D> colorMap;    // Albedo/diffuse texture
  std::shared_ptr<Vivid::Texture2D> normalMap;   // Normal map for detail
  std::shared_ptr<Vivid::Texture2D> specularMap; // Specular/roughness

  // Layer blend map (0-1 UV span across entire terrain)
  // R channel defines this layer's strength at each point
  std::shared_ptr<Vivid::Texture2D> layerMap;

  // Source paths for textures (for serialization and editor display)
  std::string colorPath;
  std::string normalPath;
  std::string specularPath;

  TerrainLayer() = default;
};

} // namespace Quantum
