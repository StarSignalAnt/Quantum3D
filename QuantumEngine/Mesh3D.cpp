#include "Mesh3D.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vulkan/vulkan.h>

namespace Quantum {

// ========== Vertex3D Static Methods ==========

VkVertexInputBindingDescription Vertex3D::GetBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex3D);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription>
Vertex3D::GetAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions(6);

  // Position
  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(Vertex3D, position);

  // Normal
  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(Vertex3D, normal);

  // UV
  attributeDescriptions[2].binding = 0;
  attributeDescriptions[2].location = 2;
  attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[2].offset = offsetof(Vertex3D, uv);

  // UV2 (Lightmap)
  attributeDescriptions[3].binding = 0;
  attributeDescriptions[3].location = 3;
  attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[3].offset = offsetof(Vertex3D, uv2);

  // Tangent
  attributeDescriptions[4].binding = 0;
  attributeDescriptions[4].location = 4;
  attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[4].offset = offsetof(Vertex3D, tangent);

  // Bitangent
  attributeDescriptions[5].binding = 0;
  attributeDescriptions[5].location = 5;
  attributeDescriptions[5].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[5].offset = offsetof(Vertex3D, bitangent);

  return attributeDescriptions;
}

// ========== Mesh3D Implementation ==========

Mesh3D::Mesh3D(const std::string &name)
    : m_Name(name), m_BoundsMin(0.0f), m_BoundsMax(0.0f) {}

Mesh3D::~Mesh3D() {
  // Buffers cleaned up via unique_ptr
}

// ========== Vertex Data Manipulation ==========

void Mesh3D::AddVertex(const Vertex3D &vertex) {
  m_Vertices.push_back(vertex);
  m_Finalized = false;
}

void Mesh3D::AddTriangle(const Triangle &triangle) {
  m_Triangles.push_back(triangle);
  m_Finalized = false;
}

void Mesh3D::AddTriangle(uint32_t v0, uint32_t v1, uint32_t v2) {
  m_Triangles.emplace_back(v0, v1, v2);
  m_Finalized = false;
}

void Mesh3D::SetVertices(const std::vector<Vertex3D> &vertices) {
  m_Vertices = vertices;
  m_Finalized = false;
}

void Mesh3D::SetTriangles(const std::vector<Triangle> &triangles) {
  m_Triangles = triangles;
  m_Finalized = false;
}

void Mesh3D::Clear() {
  m_Vertices.clear();
  m_Triangles.clear();
  m_VertexBuffer.reset();
  m_IndexBuffer.reset();
  m_Finalized = false;
  m_BoundsMin = glm::vec3(0.0f);
  m_BoundsMax = glm::vec3(0.0f);
}

// ========== Material ==========

void Mesh3D::SetMaterial(std::shared_ptr<Material> material) {
  m_Material = material;
  m_BoundsMin = glm::vec3(0.0f);
  m_BoundsMax = glm::vec3(0.0f);
}

// ========== Vulkan Buffer Creation ==========

void Mesh3D::Finalize(Vivid::VividDevice *device) {
  if (m_Vertices.empty())
    return;

  RecalculateBounds(); // Ensure bounds are computed

  if (m_VertexBuffer) {
    m_VertexBuffer.reset();
  }
  if (m_IndexBuffer) {
    m_IndexBuffer.reset();
  }

  // Create Vertex Buffer
  VkDeviceSize bufferSize = sizeof(Vertex3D) * m_Vertices.size();
  m_VertexBuffer = std::make_unique<Vivid::VividBuffer>(
      device, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  m_VertexBuffer->Map();
  m_VertexBuffer->WriteToBuffer((void *)m_Vertices.data(), bufferSize);
  m_VertexBuffer->Unmap();

  // Create Index Buffer
  std::vector<uint32_t> indices = GetIndexData();
  if (!indices.empty()) {
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    m_IndexBuffer = std::make_unique<Vivid::VividBuffer>(
        device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    m_IndexBuffer->Map();
    m_IndexBuffer->WriteToBuffer((void *)indices.data(), indexBufferSize);
    m_IndexBuffer->Unmap();

    std::cout << "[Mesh3D] Index Buffer Created: "
              << (void *)m_IndexBuffer->GetBuffer() << std::endl;
  }

  m_Finalized = true;
}

void Mesh3D::Bind(VkCommandBuffer commandBuffer) const {
  if (!m_Finalized) {
    std::cerr << "[Mesh3D] ERROR: Attempted to bind unfinalized mesh"
              << std::endl;
    return;
  }

  if (!m_VertexBuffer || !m_IndexBuffer) {
    std::cerr << "[Mesh3D] ERROR: Attempted to bind mesh with null buffers"
              << std::endl;
    return;
  }

  VkBuffer vertexBuffers[] = {m_VertexBuffer->GetBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetBuffer(), 0,
                       VK_INDEX_TYPE_UINT32);
}

void Mesh3D::UpdateVertexBuffer() {
  if (!m_Finalized || !m_VertexBuffer)
    return;

  // Assuming HOST_VISIBLE | HOST_COHERENT
  // Optimization: In a real persistent mapped scenario, we wouldn't Map/Unmap
  // every time. But for now, this avoids recreating the buffer.
  m_VertexBuffer->Map();
  m_VertexBuffer->WriteToBuffer((void *)m_Vertices.data(),
                                sizeof(Vertex3D) * m_Vertices.size());
  m_VertexBuffer->Unmap();

  // Increment version so caching systems know to rebuild
  ++m_GeometryVersion;
}

void Mesh3D::UpdateVertex(size_t index) {
  if (!m_Finalized || !m_VertexBuffer || index >= m_Vertices.size())
    return;

  size_t offset = index * sizeof(Vertex3D);
  m_VertexBuffer->WriteToBuffer((void *)&m_Vertices[index], sizeof(Vertex3D),
                                offset);
}

// ========== Utilities ==========

std::vector<uint32_t> Mesh3D::GetIndexData() const {
  std::vector<uint32_t> indices;
  indices.reserve(m_Triangles.size() * 3);
  for (const auto &tri : m_Triangles) {
    indices.push_back(tri.v0);
    indices.push_back(tri.v1);
    indices.push_back(tri.v2);
  }
  return indices;
}

void Mesh3D::RecalculateBounds() {
  if (m_Vertices.empty()) {
    m_BoundsMin = glm::vec3(0.0f);
    m_BoundsMax = glm::vec3(0.0f);
    return;
  }

  m_BoundsMin = m_Vertices[0].position;
  m_BoundsMax = m_Vertices[0].position;

  for (const auto &vertex : m_Vertices) {
    m_BoundsMin = glm::min(m_BoundsMin, vertex.position);
    m_BoundsMax = glm::max(m_BoundsMax, vertex.position);
  }
}

void Mesh3D::RecalculateNormals() {
  // Reset all normals
  for (auto &vertex : m_Vertices) {
    vertex.normal = glm::vec3(0.0f);
  }

  // Accumulate face normals
  for (const auto &tri : m_Triangles) {
    glm::vec3 v0 = m_Vertices[tri.v0].position;
    glm::vec3 v1 = m_Vertices[tri.v1].position;
    glm::vec3 v2 = m_Vertices[tri.v2].position;

    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 faceNormal = glm::cross(edge1, edge2);

    m_Vertices[tri.v0].normal += faceNormal;
    m_Vertices[tri.v1].normal += faceNormal;
    m_Vertices[tri.v2].normal += faceNormal;
  }

  // Normalize
  for (auto &vertex : m_Vertices) {
    if (glm::length(vertex.normal) > 0.0001f) {
      vertex.normal = glm::normalize(vertex.normal);
    } else {
      vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
  }
}

void Mesh3D::RecalculateTangents() {
  // Reset tangents
  for (auto &vertex : m_Vertices) {
    vertex.tangent = glm::vec3(0.0f);
    vertex.bitangent = glm::vec3(0.0f);
  }

  // Calculate tangent/bitangent per triangle
  for (const auto &tri : m_Triangles) {
    Vertex3D &v0 = m_Vertices[tri.v0];
    Vertex3D &v1 = m_Vertices[tri.v1];
    Vertex3D &v2 = m_Vertices[tri.v2];

    glm::vec3 edge1 = v1.position - v0.position;
    glm::vec3 edge2 = v2.position - v0.position;
    glm::vec2 deltaUV1 = v1.uv - v0.uv;
    glm::vec2 deltaUV2 = v2.uv - v0.uv;

    float f =
        1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y + 0.0001f);

    glm::vec3 tangent;
    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

    glm::vec3 bitangent;
    bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
    bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
    bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

    v0.tangent += tangent;
    v1.tangent += tangent;
    v2.tangent += tangent;

    v0.bitangent += bitangent;
    v1.bitangent += bitangent;
    v2.bitangent += bitangent;
  }

  // Orthonormalize
  for (size_t i = 0; i < m_Vertices.size(); i++) {
    glm::vec3 n = m_Vertices[i].normal;
    glm::vec3 t = m_Vertices[i].tangent;
    glm::vec3 b = m_Vertices[i].bitangent;

    // Gram-Schmidt orthonormalize tangent
    t = glm::normalize(t - n * glm::dot(n, t));

    if (glm::length(b) > 0.0001f)
      b = glm::normalize(b);

    m_Vertices[i].tangent = t;
    m_Vertices[i].bitangent = b;
  }
}

std::shared_ptr<Mesh3D> Mesh3D::CreateUnitCube() {
  auto mesh = std::make_shared<Mesh3D>("UnitCube");

  // 8 vertices
  // Front face
  mesh->AddVertex(Vertex3D(glm::vec3(-0.5f, -0.5f, 0.5f))); // 0: BL
  mesh->AddVertex(Vertex3D(glm::vec3(0.5f, -0.5f, 0.5f)));  // 1: BR
  mesh->AddVertex(Vertex3D(glm::vec3(0.5f, 0.5f, 0.5f)));   // 2: TR
  mesh->AddVertex(Vertex3D(glm::vec3(-0.5f, 0.5f, 0.5f)));  // 3: TL

  // Back face
  mesh->AddVertex(Vertex3D(glm::vec3(-0.5f, -0.5f, -0.5f))); // 4: BL
  mesh->AddVertex(Vertex3D(glm::vec3(0.5f, -0.5f, -0.5f)));  // 5: BR
  mesh->AddVertex(Vertex3D(glm::vec3(0.5f, 0.5f, -0.5f)));   // 6: TR
  mesh->AddVertex(Vertex3D(glm::vec3(-0.5f, 0.5f, -0.5f)));  // 7: TL

  // Indices (Triangles)
  // Front
  mesh->AddTriangle(0, 1, 2);
  mesh->AddTriangle(2, 3, 0);
  // Back
  mesh->AddTriangle(5, 4, 7);
  mesh->AddTriangle(7, 6, 5);
  // Left
  mesh->AddTriangle(4, 0, 3);
  mesh->AddTriangle(3, 7, 4);
  // Right
  mesh->AddTriangle(1, 5, 6);
  mesh->AddTriangle(6, 2, 1);
  // Top
  mesh->AddTriangle(3, 2, 6);
  mesh->AddTriangle(6, 7, 3);
  // Bottom
  mesh->AddTriangle(4, 5, 1);
  mesh->AddTriangle(1, 0, 4);

  return mesh;
}

bool Mesh3D::Intersect(const glm::mat4 &modelMatrix, const glm::vec3 &rayOrigin,
                       const glm::vec3 &rayDirection,
                       float &outDistance) const {
  const float EPSILON = 0.0000001f;
  float closestT = std::numeric_limits<float>::max();
  bool hit = false;

  for (const auto &tri : m_Triangles) {
    // Transform vertices to world space
    glm::vec3 v0 =
        glm::vec3(modelMatrix * glm::vec4(m_Vertices[tri.v0].position, 1.0f));
    glm::vec3 v1 =
        glm::vec3(modelMatrix * glm::vec4(m_Vertices[tri.v1].position, 1.0f));
    glm::vec3 v2 =
        glm::vec3(modelMatrix * glm::vec4(m_Vertices[tri.v2].position, 1.0f));

    // Möller–Trumbore ray-triangle intersection
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(rayDirection, edge2);
    float a = glm::dot(edge1, h);

    if (a > -EPSILON && a < EPSILON)
      continue; // Ray parallel to triangle

    float f = 1.0f / a;
    glm::vec3 s = rayOrigin - v0;
    float u = f * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f)
      continue;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(rayDirection, q);

    if (v < 0.0f || u + v > 1.0f)
      continue;

    float t = f * glm::dot(edge2, q);

    if (t > EPSILON && t < closestT) {
      closestT = t;
      hit = true;
    }
  }

  if (hit) {
    outDistance = closestT;
  }
  return hit;
}

// ========== Lightmap Support ==========

void Mesh3D::SetLightmapUV(size_t vertexIndex, const glm::vec2 &lightmapUV) {
  if (vertexIndex < m_Vertices.size()) {
    m_Vertices[vertexIndex].uv2 = lightmapUV;
    m_HasLightmapUVs = true;
  }
}

glm::vec2 Mesh3D::GetLightmapUV(size_t vertexIndex) const {
  if (vertexIndex < m_Vertices.size()) {
    return m_Vertices[vertexIndex].uv2;
  }
  return glm::vec2(0.0f);
}

void Mesh3D::SetLightmap(std::shared_ptr<Vivid::Texture2D> lightmap) {
  m_Lightmap = lightmap;
}

} // namespace Quantum
