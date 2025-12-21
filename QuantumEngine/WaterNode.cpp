#include "WaterNode.h"
#include "Texture2D.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace Quantum {

WaterNode::WaterNode(const std::string &name) : GraphNode(name) {
  GeneratePlaneMesh();
}

WaterNode::~WaterNode() {}

void WaterNode::Initialize(Vivid::VividDevice *device) {
  CreateWaterMaterial(device);

  // Finalize mesh (upload to GPU)
  // Do this AFTER creating material so checks pass if any
  if (HasMeshes()) {
    auto mesh = GetMeshes()[0];
    if (mesh && !mesh->IsFinalized()) {
      mesh->Finalize(device);
    }
  }
}

void WaterNode::GeneratePlaneMesh() {
  auto mesh = std::make_shared<Mesh3D>("WaterMesh");

  // Create a subdivided plane
  int gridSize = 128;    // Higher resolution for smoother waves
  float tileSize = 0.5f; // Size of each grid cell
  float size = (gridSize * tileSize) / 2.0f; // Half-width for centering
  float uvScale = 4.0f;                      // Texture tiling factor

  // Generate Vertices
  for (int z = 0; z <= gridSize; ++z) {
    for (int x = 0; x <= gridSize; ++x) {
      float px = (x * tileSize) - size;
      float pz = (z * tileSize) - size;
      float u = (float)x / (float)gridSize * uvScale;
      float v = (float)z / (float)gridSize * uvScale;

      mesh->AddVertex(Vertex3D(glm::vec3(px, 0.0f, pz), glm::vec3(0, 1, 0),
                               glm::vec2(u, v)));
    }
  }

  // Generate Indices (Triangles)
  for (int z = 0; z < gridSize; ++z) {
    for (int x = 0; x < gridSize; ++x) {
      int topLeft = z * (gridSize + 1) + x;
      int topRight = topLeft + 1;
      int bottomLeft = (z + 1) * (gridSize + 1) + x;
      int bottomRight = bottomLeft + 1;

      // Triangle 1
      mesh->AddTriangle(topLeft, bottomLeft, topRight);
      // Triangle 2
      mesh->AddTriangle(topRight, bottomLeft, bottomRight);
    }
  }

  mesh->RecalculateNormals();  // Ensure normals are normalized (though we set
                               // them)
  mesh->RecalculateTangents(); // CRITICAL: Generate tangents for normal
                               // mapping!

  AddMesh(mesh);
}

void WaterNode::CreateWaterMaterial(Vivid::VividDevice *device) {
  auto material = std::make_shared<Material>("WaterMaterial");
  material->SetPipeline("PLWater"); // Use custom water pipeline

  // Create Blue Albedo Texture
  // RGBA
  unsigned char pixel[4] = {0, 100, 200, 200}; // Sligthly darker blue
  auto albedo = std::make_shared<Vivid::Texture2D>(device, pixel, 1, 1, 4);
  material->SetAlbedoTexture(albedo);

  // Roughness (Low for shiny water)
  unsigned char roughPixel[4] = {10, 10, 10, 255};
  auto roughness =
      std::make_shared<Vivid::Texture2D>(device, roughPixel, 1, 1, 4);
  material->SetRoughnessTexture(roughness);

  // Load Normal Map
  // Ensure "engine/textures/waternm.png" exists.
  auto normalMap = std::make_shared<Vivid::Texture2D>(
      device, "engine/textures/waternm.png", VK_FORMAT_R8G8B8A8_UNORM);
  material->SetNormalTexture(normalMap);

  // Set material to mesh
  if (HasMeshes()) {
    GetMeshes()[0]->SetMaterial(material);
  }
}

} // namespace Quantum
