#include "Mesh3D.h"
#include <cstring>
#include <iostream>
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
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

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

  // Tangent
  attributeDescriptions[3].binding = 0;
  attributeDescriptions[3].location = 3;
  attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[3].offset = offsetof(Vertex3D, tangent);

  // Bitangent
  attributeDescriptions[4].binding = 0;
  attributeDescriptions[4].location = 4;
  attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[4].offset = offsetof(Vertex3D, bitangent);

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
}

// ========== Vulkan Buffer Creation ==========

void Mesh3D::Finalize(Vivid::VividDevice *device) {
  if (m_Vertices.empty() || m_Triangles.empty()) {
    throw std::runtime_error(
        "Cannot finalize mesh with no vertices or triangles");
  }

  // Calculate bounds
  RecalculateBounds();

  // Create vertex buffer
  VkDeviceSize vertexBufferSize = sizeof(Vertex3D) * m_Vertices.size();
  std::cout << "[Mesh3D] Finalizing '" << m_Name << "': " << m_Vertices.size()
            << " vertices (" << vertexBufferSize << " bytes)" << std::endl;

  m_VertexBuffer = std::make_unique<Vivid::VividBuffer>(
      device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_VertexBuffer->Map();
  m_VertexBuffer->WriteToBuffer((void *)m_Vertices.data(), vertexBufferSize);
  m_VertexBuffer->Unmap();

  std::cout << "[Mesh3D] Vertex Buffer Created: "
            << (void *)m_VertexBuffer->GetBuffer() << std::endl;

  // Create index buffer
  std::vector<uint32_t> indices = GetIndexData();
  VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
  std::cout << "[Mesh3D] Finalizing '" << m_Name << "': " << indices.size()
            << " indices (" << indexBufferSize << " bytes)" << std::endl;

  m_IndexBuffer = std::make_unique<Vivid::VividBuffer>(
      device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_IndexBuffer->Map();
  m_IndexBuffer->WriteToBuffer((void *)indices.data(), indexBufferSize);
  m_IndexBuffer->Unmap();

  std::cout << "[Mesh3D] Index Buffer Created: "
            << (void *)m_IndexBuffer->GetBuffer() << std::endl;

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

  m_Finalized = false;
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

    // Handedness check not strictly needed if we store explicit bitangent,
    // but good to keep orthogonal.
    // For now just normalize bitangent too.
    // Note: To be perfectly accurate we should re-compute bitangent as cross(n,
    // t) * handedness but if we are just storing what was accumulated:
    if (glm::length(b) > 0.0001f)
      b = glm::normalize(b);

    m_Vertices[i].tangent = t;
    m_Vertices[i].bitangent = b;
  }

  m_Finalized = false;
}

} // namespace Quantum
