#pragma once
#include "Material.h"
#include "VividBuffer.h"
#include "VividDevice.h"
#include "glm/glm.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Quantum {

/// <summary>
/// Vertex data for 3D meshes with position, normal, UV, and tangent.
/// </summary>
struct Vertex3D {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::vec3 tangent;
  glm::vec3 bitangent;

  Vertex3D()
      : position(0.0f), normal(0.0f, 1.0f, 0.0f), uv(0.0f), tangent(0.0f),
        bitangent(0.0f) {}

  Vertex3D(const glm::vec3 &pos)
      : position(pos), normal(0.0f, 1.0f, 0.0f), uv(0.0f), tangent(0.0f),
        bitangent(0.0f) {}

  Vertex3D(const glm::vec3 &pos, const glm::vec3 &norm,
           const glm::vec2 &texCoord)
      : position(pos), normal(norm), uv(texCoord), tangent(0.0f),
        bitangent(0.0f) {}

  // Vulkan vertex input binding description
  static VkVertexInputBindingDescription GetBindingDescription();
  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions();
};

/// <summary>
/// Triangle defined by three vertex indices.
/// </summary>
struct Triangle {
  uint32_t v0, v1, v2;

  Triangle() : v0(0), v1(0), v2(0) {}
  Triangle(uint32_t a, uint32_t b, uint32_t c) : v0(a), v1(b), v2(c) {}
};

/// <summary>
/// A 3D mesh composed of vertices and triangles.
/// Each mesh references a material. A node can have multiple meshes (one per
/// material). Call Finalize() to create Vulkan buffers from vertex/triangle
/// data.
/// </summary>
class Mesh3D {
public:
  Mesh3D(const std::string &name = "Mesh");
  ~Mesh3D();

  static std::shared_ptr<Mesh3D> CreateUnitCube();

  // Name
  const std::string &GetName() const { return m_Name; }
  void SetName(const std::string &name) { m_Name = name; }

  // Vertex data manipulation (before Finalize)
  void AddVertex(const Vertex3D &vertex);
  void AddTriangle(const Triangle &triangle);
  void AddTriangle(uint32_t v0, uint32_t v1, uint32_t v2);
  void SetVertices(const std::vector<Vertex3D> &vertices);
  void SetTriangles(const std::vector<Triangle> &triangles);
  void Clear();

  // Access vertex data
  const std::vector<Vertex3D> &GetVertices() const { return m_Vertices; }
  const std::vector<Triangle> &GetTriangles() const { return m_Triangles; }
  size_t GetVertexCount() const { return m_Vertices.size(); }
  size_t GetTriangleCount() const { return m_Triangles.size(); }
  size_t GetIndexCount() const { return m_Triangles.size() * 3; }

  // Material
  void SetMaterial(std::shared_ptr<Material> material);
  std::shared_ptr<Material> GetMaterial() const { return m_Material; }

  // Finalize - creates Vulkan buffers from vertex/triangle data
  void Finalize(Vivid::VividDevice *device);
  bool IsFinalized() const { return m_Finalized; }

  // Robust check for valid buffers
  bool IsValid() const {
    return m_Finalized && m_VertexBuffer && m_IndexBuffer;
  }

  // Vulkan buffer access (after Finalize)
  VkBuffer GetVertexBuffer() const {
    return m_VertexBuffer ? m_VertexBuffer->GetBuffer() : VK_NULL_HANDLE;
  }
  VkBuffer GetIndexBuffer() const {
    return m_IndexBuffer ? m_IndexBuffer->GetBuffer() : VK_NULL_HANDLE;
  }

  // Bind buffers to command buffer for rendering
  void Bind(VkCommandBuffer commandBuffer) const;

  // Calculate normals from triangle data
  void RecalculateNormals();

  // Calculate tangents for normal mapping
  void RecalculateTangents();

  // Bounding box
  glm::vec3 GetBoundsMin() const { return m_BoundsMin; }
  glm::vec3 GetBoundsMax() const { return m_BoundsMax; }
  void RecalculateBounds();

  // Ray-mesh intersection for picking
  // Returns true if ray hits mesh, with distance in outDistance
  bool Intersect(const glm::mat4 &modelMatrix, const glm::vec3 &rayOrigin,
                 const glm::vec3 &rayDirection, float &outDistance) const;

private:
  std::string m_Name;

  // CPU-side vertex data
  std::vector<Vertex3D> m_Vertices;
  std::vector<Triangle> m_Triangles;

  // Material
  std::shared_ptr<Material> m_Material;

  // GPU buffers (created on Finalize)
  std::unique_ptr<Vivid::VividBuffer> m_VertexBuffer;
  std::unique_ptr<Vivid::VividBuffer> m_IndexBuffer;
  bool m_Finalized = false;

  // Bounds
  glm::vec3 m_BoundsMin;
  glm::vec3 m_BoundsMax;

  // Helpers
  std::vector<uint32_t> GetIndexData() const;
};

} // namespace Quantum
