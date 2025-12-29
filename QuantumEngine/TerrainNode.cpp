#include "TerrainNode.h"
#include "Material.h"
#include "Mesh3D.h"
#include "Texture2D.h"
#include <iostream>
#include <vulkan/vulkan.h>

namespace Quantum {

// Helper to clamp int values
static int clampInt(int value, int minVal, int maxVal) {
  if (value < minVal)
    return minVal;
  if (value > maxVal)
    return maxVal;
  return value;
}

TerrainNode::TerrainNode(const std::string &name, float width, float depth,
                         int divisions, int layerCount)
    : GraphNode(name), m_Width(width), m_Depth(depth), m_Divisions(divisions),
      m_LayerCount(clampInt(layerCount, 1, 4)) {
  m_Layers.resize(m_LayerCount);
  InitializeBlendMaps(); // Ensure blend data is ready
  GenerateTerrainMesh();
}

TerrainNode::~TerrainNode() {}

void TerrainNode::Initialize(Vivid::VividDevice *device) {
  m_Device = device; // Store for later texture loading
  CreateDefaultTextures(device);

  // Finalize mesh (upload to GPU)
  if (HasMeshes()) {
    auto mesh = GetMeshes()[0];
    if (mesh && !mesh->IsFinalized()) {
      mesh->Finalize(device);
    }
  }
}

TerrainLayer &TerrainNode::GetLayer(int index) {
  return m_Layers[clampInt(index, 0, m_LayerCount - 1)];
}

const TerrainLayer &TerrainNode::GetLayer(int index) const {
  return m_Layers[clampInt(index, 0, m_LayerCount - 1)];
}

std::shared_ptr<GraphNode> TerrainNode::Clone() {
  auto clone = std::make_shared<TerrainNode>(
      GetName() + "_Copy", m_Width, m_Depth, m_Divisions, m_LayerCount);
  CopyTo(clone.get());
  // Copy layer references
  for (int i = 0; i < m_LayerCount; ++i) {
    clone->m_Layers[i] = m_Layers[i];
  }
  return clone;
}

void TerrainNode::GenerateTerrainMesh() {
  auto mesh = std::make_shared<Mesh3D>("TerrainMesh");

  // Calculate half dimensions (origin is center)
  float halfWidth = m_Width / 2.0f;
  float halfDepth = m_Depth / 2.0f;

  // Grid step sizes
  float stepX = m_Width / static_cast<float>(m_Divisions);
  float stepZ = m_Depth / static_cast<float>(m_Divisions);

  // Generate vertices
  // Vertices go from -halfWidth to +halfWidth (X)
  //            and   -halfDepth to +halfDepth (Z)
  for (int z = 0; z <= m_Divisions; ++z) {
    for (int x = 0; x <= m_Divisions; ++x) {
      float px = -halfWidth + (x * stepX);
      float pz = -halfDepth + (z * stepZ);

      // UV coordinates: 0-1 across entire terrain (for layer maps)
      float u = static_cast<float>(x) / static_cast<float>(m_Divisions);
      float v = static_cast<float>(z) / static_cast<float>(m_Divisions);

      // Flat terrain (Y = 0) - heightmap support can be added later
      mesh->AddVertex(Vertex3D(glm::vec3(px, 0.0f, pz),
                               glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(u, v)));
    }
  }

  // Generate triangles (indices)
  for (int z = 0; z < m_Divisions; ++z) {
    for (int x = 0; x < m_Divisions; ++x) {
      int topLeft = z * (m_Divisions + 1) + x;
      int topRight = topLeft + 1;
      int bottomLeft = (z + 1) * (m_Divisions + 1) + x;
      int bottomRight = bottomLeft + 1;

      // Two triangles per grid cell
      mesh->AddTriangle(topLeft, bottomLeft, topRight);
      mesh->AddTriangle(topRight, bottomLeft, bottomRight);
    }
  }

  mesh->RecalculateNormals();
  mesh->RecalculateTangents();

  AddMesh(mesh);
}

void TerrainNode::CreateDefaultTextures(Vivid::VividDevice *device) {
  // Create a default material for the terrain mesh
  auto material = std::make_shared<Material>("TerrainMaterial");
  material->SetPipeline("PLTerrain");

  // Default flat normal (pointing up in tangent space): (0,0,1) encoded as
  // (128,128,255)
  unsigned char defaultNormal[4] = {128, 128, 255, 255};

  // Default specular: 0.5 grey
  unsigned char defaultSpec[4] = {128, 128, 128, 255};

  // Default white color for layers without textures
  unsigned char whiteColor[4] = {255, 255, 255, 255};

  // Layer maps: layer 0 full strength, others zero
  unsigned char fullStrength[4] = {255, 255, 255, 255};
  unsigned char zeroStrength[4] = {0, 0, 0, 255};

  for (int i = 0; i < m_LayerCount; ++i) {
    // Color map: Layer 0 loads grid.png, others are white
    if (i == 0) {
      m_Layers[i].colorMap = std::make_shared<Vivid::Texture2D>(
          device, "engine/textures/grid.png");
      m_Layers[i].colorPath = "engine/textures/grid.png";
      // Fallback to white if loading failed
      if (!m_Layers[i].colorMap ||
          m_Layers[i].colorMap->GetImageView() == VK_NULL_HANDLE) {
        m_Layers[i].colorMap =
            std::make_shared<Vivid::Texture2D>(device, whiteColor, 1, 1, 4);
      }
    } else {
      m_Layers[i].colorMap =
          std::make_shared<Vivid::Texture2D>(device, whiteColor, 1, 1, 4);
    }

    // Normal map: flat normal (0,0,1) for all layers
    // MUST use UNORM format, NOT SRGB - SRGB applies gamma which corrupts
    // normals
    m_Layers[i].normalMap = std::make_shared<Vivid::Texture2D>(
        device, defaultNormal, 1, 1, 4, VK_FORMAT_R8G8B8A8_UNORM);

    // Specular map: 0.5 grey for all layers
    m_Layers[i].specularMap =
        std::make_shared<Vivid::Texture2D>(device, defaultSpec, 1, 1, 4);

    // Layer map: Initialize with BlendData (512x512)
    // Note: InitializeBlendMaps() is called in constructor, so data is ready.
    if (i < m_LayerBlendData.size()) {
      m_Layers[i].layerMap =
          std::make_shared<Vivid::Texture2D>(device, m_LayerBlendData[i].data(),
                                             m_BlendMapSize, m_BlendMapSize, 4);
    } else {
      // Fallback (should not happen)
      m_Layers[i].layerMap =
          std::make_shared<Vivid::Texture2D>(device, zeroStrength, 1, 1, 4);
    }
  }

  // Set material to mesh
  if (HasMeshes()) {
    GetMeshes()[0]->SetMaterial(material);
  }
}

void TerrainNode::SetLayerTexture(int layerIndex, const std::string &type,
                                  const std::string &path) {
  if (layerIndex < 0 || layerIndex >= m_LayerCount) {
    return;
  }

  // Thread-safe: push to pending updates
  std::lock_guard<std::mutex> lock(m_UpdatesMutex);
  m_PendingUpdates.push_back({layerIndex, type, path});

  // Note: We don't mark m_DescriptorDirty here, it happens in
  // ProcessPendingUpdates
}

void TerrainNode::ProcessPendingUpdates() {
  std::lock_guard<std::mutex> lock(m_UpdatesMutex);
  if (m_PendingUpdates.empty() || !m_Device) {
    return;
  }

  // Wait for GPU to finish using the current textures/descriptors
  // This is safer on the render thread before we record new commands
  vkDeviceWaitIdle(m_Device->GetDevice());

  for (const auto &update : m_PendingUpdates) {
    TerrainLayer &layer = m_Layers[update.layer];

    if (update.type == "color") {
      layer.colorPath = update.path;
      layer.colorMap =
          std::make_shared<Vivid::Texture2D>(m_Device, update.path);
    } else if (update.type == "normal") {
      layer.normalPath = update.path;
      // Use UNORM for normal maps to avoid gamma correction
      layer.normalMap = std::make_shared<Vivid::Texture2D>(
          m_Device, update.path, VK_FORMAT_R8G8B8A8_UNORM);
    } else if (update.type == "specular") {
      layer.specularPath = update.path;
      layer.specularMap =
          std::make_shared<Vivid::Texture2D>(m_Device, update.path);
    }
  }

  m_PendingUpdates.clear();
  m_DescriptorDirty = true;
}

void TerrainNode::InitializeBlendMaps() {
  m_LayerBlendData.resize(m_LayerCount);
  m_LayerDirty.resize(m_LayerCount, false);

  size_t dataSize = m_BlendMapSize * m_BlendMapSize * 4; // RGBA

  for (int i = 0; i < m_LayerCount; ++i) {
    m_LayerBlendData[i].resize(dataSize, 0);
    // Initialize layer 0 to full strength (red channel = 255), others to 0
    if (i == 0) {
      for (size_t j = 0; j < dataSize; j += 4) {
        m_LayerBlendData[i][j] = 255;     // R
        m_LayerBlendData[i][j + 1] = 0;   // G
        m_LayerBlendData[i][j + 2] = 0;   // B
        m_LayerBlendData[i][j + 3] = 255; // A
      }
      m_LayerDirty[i] = true;
    } else {
      // Initialize others to 0 (R=0)
      for (size_t j = 0; j < dataSize; j += 4) {
        m_LayerBlendData[i][j] = 0;       // R
        m_LayerBlendData[i][j + 1] = 0;   // G
        m_LayerBlendData[i][j + 2] = 0;   // B
        m_LayerBlendData[i][j + 3] = 255; // A
      }
      m_LayerDirty[i] = true;
    }
  }
}

void TerrainNode::OnUpdate(float dt) {
  ProcessPendingUpdates();
  UpdateGPUTextures();
}

void TerrainNode::UpdateGPUTextures() {
  if (!m_AnyLayerDirty)
    return; // Optimization: Skip if no changes

  for (int i = 0; i < m_LayerCount; ++i) {
    if (m_LayerDirty[i]) {
      if (i < m_Layers.size() && m_Layers[i].layerMap) {
        m_Layers[i].layerMap->SetPixels(m_LayerBlendData[i]);
      }
      m_LayerDirty[i] = false;
    }
  }
  m_AnyLayerDirty = false;
}

void TerrainNode::Paint(const glm::vec3 &hitPoint, int layerIndex, float radius,
                        float strength) {
  if (layerIndex < 0 || layerIndex >= m_LayerCount)
    return;

  // Convert world hit point to UV space
  // Terrain is centered at (0,0,0)
  float halfWidth = m_Width / 2.0f;
  float halfDepth = m_Depth / 2.0f;

  float u = (hitPoint.x + halfWidth) / m_Width;
  float v = (hitPoint.z + halfDepth) / m_Depth;
  // Convert UV to pixel coords
  int centerX = static_cast<int>(u * m_BlendMapSize);
  int centerY = static_cast<int>(v * m_BlendMapSize);
  if (layerIndex < 0 || layerIndex >= m_LayerCount) {
    std::cerr << "[TerrainNode] Error: Invalid layer index " << layerIndex
              << " for paint operation!" << std::endl;
    return;
  }

  if (layerIndex >= m_LayerBlendData.size()) {
    std::cerr << "[TerrainNode] Error: Layer index " << layerIndex
              << " exceeds BlendData size " << m_LayerBlendData.size()
              << std::endl;
    return;
  }

  int pixelRadius = static_cast<int>((radius / m_Width) * m_BlendMapSize);

  // Square of pixel radius for distance check
  int distSqLimit = pixelRadius * pixelRadius;

  int minX = std::max(0, centerX - pixelRadius);
  int maxX = std::min(m_BlendMapSize - 1, centerX + pixelRadius);
  int minY = std::max(0, centerY - pixelRadius);
  int maxY = std::min(m_BlendMapSize - 1, centerY + pixelRadius);

  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      int distSq =
          (x - centerX) * (x - centerX) + (y - centerY) * (y - centerY);
      if (distSq <= distSqLimit) {
        // Calculate falloff (linear for now)
        float currentDist = std::sqrt(static_cast<float>(distSq));
        float falloff = 1.0f - (currentDist / pixelRadius);
        // Apply curve? Smoothstep? For now just linear.
        if (falloff < 0)
          falloff = 0;

        float weightChange = strength * falloff;

        // Apply to target layer
        size_t idx = (y * m_BlendMapSize + x) * 4;

        // Current value (0-255)
        float currentVal = m_LayerBlendData[layerIndex][idx];
        float newVal = currentVal + weightChange * 255.0f;
        newVal = std::min(255.0f, newVal);
        m_LayerBlendData[layerIndex][idx] = static_cast<unsigned char>(newVal);
        m_LayerDirty[layerIndex] = true;
        m_AnyLayerDirty = true;

        // Subtract from other layers proportionally or just radially
        // To keep sum ~ 255.

        // Simple strategy: reduce others by the amount we added * their
        // relative contribution? Or just reduce them. To ensure conservation
        // of 1.0 (255):
        // 1. Calculate sum of ALL layers (using new target value)
        // 2. Normalize.

        float totalWeight = 0.0f;
        for (int l = 0; l < m_LayerCount; ++l) {
          totalWeight += m_LayerBlendData[l][idx];
        }

        if (totalWeight > 0.1f) {
          for (int l = 0; l < m_LayerCount; ++l) {
            float w = m_LayerBlendData[l][idx];
            w = (w / totalWeight) * 255.0f;
            m_LayerBlendData[l][idx] = static_cast<unsigned char>(w);
            m_LayerDirty[l] = true;
            m_AnyLayerDirty = true;
          }
        }
      }
    }
  }
}

void TerrainNode::Sculpt(const glm::vec3 &hitPoint, float radius,
                         float strength) {
  const auto &meshes = GetMeshes();
  if (meshes.empty())
    return;
  auto mesh = meshes[0];
  if (!mesh)
    return;

  // We modify the mesh vertices directly
  // Need to access vertices non-const
  // This is a bit of a hack since GetVertices is const, but for performance we
  // rely on mesh knowing how to update Actually, Mesh3D exposes AddVertex but
  // not direct modification easily EXCEPT by SetVertices But we added
  // UpdateVertexBuffer which uses m_Vertices. So we need mutable access to
  // vertices. Let's const_cast for now to avoid changing the whole Mesh3D API
  // or copy the vector (expensive) Ideally Mesh3D should expose
  // `GetVerticesMutable()` to Mesh3D. Or just use friend/const_cast hack for
  // speed in this iteration
  std::vector<Vertex3D> &verts =
      const_cast<std::vector<Vertex3D> &>(mesh->GetVertices());

  // Convert hitPoint to local space
  glm::mat4 inverseModel = glm::inverse(GetWorldMatrix());
  glm::vec3 localHit = glm::vec3(inverseModel * glm::vec4(hitPoint, 1.0f));

  // Iterate over all vertices? No, that's O(N).
  // Terrain is a grid. We can find indices directly.
  // Grid size: m_Divisions X m_Divisions
  // Width/Depth

  float cellW = m_Width / m_Divisions;
  float cellD = m_Depth / m_Divisions;

  float halfWidth = m_Width / 2.0f;
  float halfDepth = m_Depth / 2.0f;

  float brushX = localHit.x + halfWidth;
  float brushY = localHit.z + halfDepth;

  int centerX = (int)(brushX / cellW);
  int centerY = (int)(brushY / cellD);

  int gridRadius = (int)(radius / cellW) + 1;

  int minX = std::max(0, centerX - gridRadius);
  int maxX = std::min(m_Divisions, centerX + gridRadius);
  int minY = std::max(0, centerY - gridRadius);
  int maxY = std::min(m_Divisions, centerY + gridRadius);

  bool changed = false;

  // Radius squared for distance check
  float r2 = radius * radius;

  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      // Index in vertex array
      // Vertices are generated row by row: (z * (div+1)) + x
      // size is (div+1) * (div+1)
      int vIdx = y * (m_Divisions + 1) + x;
      if (vIdx >= verts.size())
        continue;

      Vertex3D &v = verts[vIdx];

      float dx = v.position.x - localHit.x;
      float dz = v.position.z - localHit.z;
      float distSq = dx * dx + dz * dz;

      if (distSq < r2) {
        float dist = std::sqrt(distSq);
        float falloff = 1.0f - (dist / radius);
        falloff = falloff * falloff * (3.0f - 2.0f * falloff); // Smoothstep

        // Apply height change
        // strength is unit/sec? or unit per click? assuming per click for now
        v.position.y += strength * falloff * 0.5f; // Scale down a bit
        changed = true;
      }
    }
  }

  if (changed) {
    // Very expensive to recalculate all normals
    // Optimization: Only recalc normals for affected area
    // For now, call RecalculateNormals() which does all
    // To optimize, Mesh3D needs a PartialRecalculateNormals(minX, maxX...)
    mesh->RecalculateNormals();

    // Upload to GPU
    mesh->UpdateVertexBuffer();
  }
}

} // namespace Quantum
