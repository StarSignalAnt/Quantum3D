#include "GizmoBase.h"
#include "GraphNode.h"
#include "glm/gtc/matrix_transform.hpp"
#include <limits>

namespace Quantum {

Ray GizmoBase::CalculatePickingRay(int mouseX, int mouseY) const {
  Ray ray;

  if (m_ViewportWidth == 0 || m_ViewportHeight == 0) {
    return ray;
  }

  // 1. Calculate Normalized Device Coordinates (NDC)
  float x = (2.0f * mouseX) / m_ViewportWidth - 1.0f;
  float y = (2.0f * mouseY) / m_ViewportHeight - 1.0f;

  glm::vec4 ray_clip = glm::vec4(x, y, -1.0, 1.0);

  // 2. Unproject to View Space - create projection matrix matching
  // SceneRenderer
  glm::mat4 proj = glm::perspective(
      glm::radians(45.0f), (float)m_ViewportWidth / (float)m_ViewportHeight,
      0.1f, 100.0f);
  proj[1][1] *= -1; // Match Vulkan Y-flip

  glm::vec4 ray_eye = glm::inverse(proj) * ray_clip;
  ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0); // Forward is -Z

  // 3. Unproject to World Space
  glm::mat4 invView = glm::inverse(m_ViewMatrix);

  glm::vec3 ray_wor = glm::vec3(invView * ray_eye);
  ray_wor = glm::normalize(ray_wor);

  ray.origin = glm::vec3(invView[3]); // Camera position
  ray.direction = ray_wor;

  return ray;
}

bool GizmoBase::RayTriangleIntersection(const Ray &ray, const glm::vec3 &v0,
                                        const glm::vec3 &v1,
                                        const glm::vec3 &v2, float &t) {
  const float EPSILON = 0.0000001f;
  glm::vec3 edge1, edge2, h, s, q;
  float a, f, u, v;

  edge1 = v1 - v0;
  edge2 = v2 - v0;
  h = glm::cross(ray.direction, edge2);
  a = glm::dot(edge1, h);

  if (a > -EPSILON && a < EPSILON)
    return false;

  f = 1.0f / a;
  s = ray.origin - v0;
  u = f * glm::dot(s, h);

  if (u < 0.0f || u > 1.0f)
    return false;

  q = glm::cross(s, edge1);
  v = f * glm::dot(ray.direction, q);

  if (v < 0.0f || u + v > 1.0f)
    return false;

  t = f * glm::dot(edge2, q);

  return (t > EPSILON);
}

MeshHitResult GizmoBase::HitTestMesh(const Ray &ray, Mesh3D *mesh,
                                     const glm::mat4 &modelMatrix) const {
  MeshHitResult result;
  result.hit = false;
  result.distance = std::numeric_limits<float>::max();

  if (!mesh) {
    return result;
  }

  const auto &vertices = mesh->GetVertices();
  const auto &triangles = mesh->GetTriangles();

  for (const auto &tri : triangles) {
    // Transform vertices to world space
    glm::vec3 v0 =
        glm::vec3(modelMatrix * glm::vec4(vertices[tri.v0].position, 1.0f));
    glm::vec3 v1 =
        glm::vec3(modelMatrix * glm::vec4(vertices[tri.v1].position, 1.0f));
    glm::vec3 v2 =
        glm::vec3(modelMatrix * glm::vec4(vertices[tri.v2].position, 1.0f));

    float t = 0.0f;
    if (RayTriangleIntersection(ray, v0, v1, v2, t)) {
      if (t > 0.0f && t < result.distance) {
        result.hit = true;
        result.distance = t;
      }
    }
  }

  return result;
}

float GizmoBase::CalculateScreenConstantScale(float baseScale) const {
  glm::vec3 cameraPos = GetCameraPosition();
  float distance = glm::length(cameraPos - m_Position);
  float scale = baseScale * distance;
  return glm::clamp(scale, 0.01f, 100.0f);
}

glm::vec3 GizmoBase::GetCameraPosition() const {
  glm::mat4 invView = glm::inverse(m_ViewMatrix);
  return glm::vec3(invView[3]);
}

glm::mat4 GizmoBase::GetGizmoRotation() const {
  if (m_Space == GizmoSpace::Global) {
    // Global space: no rotation, axes aligned to world
    return glm::mat4(1.0f);
  }

  // Local space: use target node's rotation
  auto target = m_TargetNode.lock();
  if (target) {
    // Extract rotation from the world matrix (remove translation and scale)
    glm::mat4 worldMatrix = target->GetWorldMatrix();

    // Extract just the rotation (upper-left 3x3, normalized)
    glm::vec3 col0 = glm::normalize(glm::vec3(worldMatrix[0]));
    glm::vec3 col1 = glm::normalize(glm::vec3(worldMatrix[1]));
    glm::vec3 col2 = glm::normalize(glm::vec3(worldMatrix[2]));

    glm::mat4 rotation(1.0f);
    rotation[0] = glm::vec4(col0, 0.0f);
    rotation[1] = glm::vec4(col1, 0.0f);
    rotation[2] = glm::vec4(col2, 0.0f);
    return rotation;
  }

  return glm::mat4(1.0f);
}

} // namespace Quantum
