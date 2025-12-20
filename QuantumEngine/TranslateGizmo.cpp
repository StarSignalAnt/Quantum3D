#include "TranslateGizmo.h"
#include "GraphNode.h"
#include "RenderingPipelines.h"
#include "SceneRenderer.h"
#include "glm/gtc/matrix_transform.hpp"
#include <iostream>
#include <limits>

namespace Quantum {

TranslateGizmo::TranslateGizmo(Vivid::VividDevice *device) {
  GenerateMeshes(device);
}

bool TranslateGizmo::OnMouseClicked(int x, int y, bool isPressed, int width,
                                    int height) {
  if (isPressed) {
    // Check if we hit an axis
    GizmoAxis hit = HitTest(x, y, width, height);
    if (hit != GizmoAxis::None) {
      m_IsDragging = true;
      m_ActiveAxis = hit;
      m_LastMousePos = glm::vec2(x, y);

      // Store starting position of target node
      auto target = GetTargetNode();
      if (target) {
        m_DragStartNodePos = target->GetWorldPosition();

        // Capture axis direction at drag start (use consistently throughout
        // drag)
        m_DragAxisDirection = GetAxisDirection(m_ActiveAxis);

        // Calculate initial t-parameter along the axis where the ray intersects
        Ray ray = CalculatePickingRay(x, y);
        glm::vec3 axisOrigin = m_DragStartNodePos;

        // Find closest point on axis line to the ray (ray-line closest point)
        // Line: P = axisOrigin + t * axisDir
        // Ray: Q = ray.origin + s * ray.direction
        glm::vec3 w0 = axisOrigin - ray.origin;
        float a = glm::dot(m_DragAxisDirection, m_DragAxisDirection);
        float b = glm::dot(m_DragAxisDirection, ray.direction);
        float c = glm::dot(ray.direction, ray.direction);
        float d = glm::dot(m_DragAxisDirection, w0);
        float e = glm::dot(ray.direction, w0);
        float denom = a * c - b * b;

        if (std::abs(denom) > 0.0001f) {
          m_DragStartAxisT = (b * e - c * d) / denom;
        } else {
          m_DragStartAxisT = 0.0f;
        }
      }

      return true; // Consumed the click
    }
  } else {
    // Mouse released
    if (m_IsDragging) {
      m_IsDragging = false;
      m_ActiveAxis = GizmoAxis::None;
      return true; // Consumed the release
    }
  }
  return false; // Did not consume
}

void TranslateGizmo::OnMouseMoved(int x, int y) {
  if (!m_IsDragging || m_ActiveAxis == GizmoAxis::None)
    return;

  auto target = GetTargetNode();
  if (!target)
    return;

  // Use the axis direction captured at drag start (stored in
  // m_DragAxisDirection)
  glm::vec3 axisOrigin = m_DragStartNodePos;

  // Cast ray from current mouse position
  Ray ray = CalculatePickingRay(x, y);

  // Find closest point on axis line to the ray (ray-line closest point)
  glm::vec3 w0 = axisOrigin - ray.origin;
  float a = glm::dot(m_DragAxisDirection, m_DragAxisDirection);
  float b = glm::dot(m_DragAxisDirection, ray.direction);
  float c = glm::dot(ray.direction, ray.direction);
  float d = glm::dot(m_DragAxisDirection, w0);
  float e = glm::dot(ray.direction, w0);
  float denom = a * c - b * b;

  if (std::abs(denom) > 0.0001f) {
    float currentT = (b * e - c * d) / denom;

    // Calculate delta t and apply to node position
    float deltaT = currentT - m_DragStartAxisT;

    // Move the target node along the axis
    glm::vec3 newPos = m_DragStartNodePos + m_DragAxisDirection * deltaT;
    target->SetLocalPosition(newPos);

    // Explicitly update gizmo position during drag so it remains snappy
    m_Position = target->GetWorldPosition();
  }
}

GizmoAxis TranslateGizmo::HitTest(int mouseX, int mouseY, int width,
                                  int height) {
  SyncWithTarget();

  // Update viewport dimensions
  m_ViewportWidth = width;
  m_ViewportHeight = height;

  if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
    return GizmoAxis::None;

  // Use base class ray calculation
  Ray ray = CalculatePickingRay(mouseX, mouseY);

  // Calculate gizmo model matrix (position + rotation + scale)
  float scale = m_CurrentScale;
  if (scale < 0.001f) {
    scale = CalculateScreenConstantScale();
  }
  glm::mat4 rotation = GetGizmoRotation();
  glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), m_Position);
  modelMatrix = modelMatrix * rotation; // Apply local/global rotation
  modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));

  std::cout << "  Ray origin: " << ray.origin.x << ", " << ray.origin.y << ", "
            << ray.origin.z << std::endl;
  std::cout << "  Ray dir: " << ray.direction.x << ", " << ray.direction.y
            << ", " << ray.direction.z << std::endl;
  std::cout << "  Gizmo center: " << m_Position.x << ", " << m_Position.y
            << ", " << m_Position.z << std::endl;
  std::cout << "  Scale: " << scale << std::endl;

  // Test each axis using base class hit test
  float closestDist = std::numeric_limits<float>::max();
  GizmoAxis closestAxis = GizmoAxis::None;

  auto testAxis = [&](Mesh3D *mesh, GizmoAxis axis, const char *name) {
    MeshHitResult result = HitTestMesh(ray, mesh, modelMatrix);
    if (result.hit && result.distance < closestDist) {
      closestDist = result.distance;
      closestAxis = axis;
      std::cout << "  HIT " << name << " at t=" << result.distance << std::endl;
    }
  };

  testAxis(m_AxisX.get(), GizmoAxis::X, "X");
  testAxis(m_AxisY.get(), GizmoAxis::Y, "Y");
  testAxis(m_AxisZ.get(), GizmoAxis::Z, "Z");

  return closestAxis;
}

glm::vec3 TranslateGizmo::GetAxisDirection(GizmoAxis axis) const {
  // Get base axis
  glm::vec3 baseAxis;
  switch (axis) {
  case GizmoAxis::X:
    baseAxis = glm::vec3(1, 0, 0);
    break;
  case GizmoAxis::Y:
    baseAxis = glm::vec3(0, 1, 0);
    break;
  case GizmoAxis::Z:
    baseAxis = glm::vec3(0, 0, 1);
    break;
  default:
    return glm::vec3(0);
  }

  // Transform by current gizmo rotation (local/global space)
  glm::mat4 rotation = GetGizmoRotation();
  return glm::normalize(glm::vec3(rotation * glm::vec4(baseAxis, 0.0f)));
}

glm::vec3 TranslateGizmo::ProjectMouseToAxis(int mouseX, int mouseY,
                                             GizmoAxis axis) {
  // Placeholder - full implementation would use proper ray-axis projection
  return m_Position;
}

void TranslateGizmo::GenerateMeshes(Vivid::VividDevice *device) {
  // Helper to create an arrow mesh pointing in 'dir' (Unit length 1.0)
  auto createArrow = [&](const std::string &name, const glm::vec3 &dir,
                         const glm::vec3 &color) -> std::shared_ptr<Mesh3D> {
    auto mesh = std::make_shared<Mesh3D>(name);

    // Arrow Shaft (Box)
    // Length 0.8, Thickness 0.08
    glm::vec3 up =
        (std::abs(dir.y) > 0.9f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    glm::vec3 right = glm::cross(dir, up);
    up = glm::cross(right, dir);

    // Normalize
    right = glm::normalize(right);
    up = glm::normalize(up);

    float shaftLen = 0.8f;
    float shaftThick = 0.025f; // Original thin shaft
    float headLen = 0.2f;      // Total 1.0
    float headThick = 0.06f;   // Original thin arrowhead

    auto addQuad = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3) {
      uint32_t i = (uint32_t)mesh->GetVertexCount();
      mesh->AddVertex(Vertex3D(v0, glm::vec3(0), glm::vec2(0)));
      mesh->AddVertex(Vertex3D(v1, glm::vec3(0), glm::vec2(0)));
      mesh->AddVertex(Vertex3D(v2, glm::vec3(0), glm::vec2(0)));
      mesh->AddVertex(Vertex3D(v3, glm::vec3(0), glm::vec2(0)));
      mesh->AddTriangle(i, i + 1, i + 2);
      mesh->AddTriangle(i, i + 2, i + 3);
    };

    // Shaft Vertices
    glm::vec3 s0 = -right * shaftThick - up * shaftThick;
    glm::vec3 s1 = right * shaftThick - up * shaftThick;
    glm::vec3 s2 = right * shaftThick + up * shaftThick;
    glm::vec3 s3 = -right * shaftThick + up * shaftThick;
    glm::vec3 e0 = s0 + dir * shaftLen;
    glm::vec3 e1 = s1 + dir * shaftLen;
    glm::vec3 e2 = s2 + dir * shaftLen;
    glm::vec3 e3 = s3 + dir * shaftLen;

    // Shaft Faces
    addQuad(s0, s1, e1, e0); // Bottom (-Up)
    addQuad(s1, s2, e2, e1); // Right (+Right)
    addQuad(s2, s3, e3, e2); // Top (+Up)
    addQuad(s3, s0, e0, e3); // Left (+Left)

    // Caps
    addQuad(s1, s0, s3, s2); // Start Cap
    addQuad(e0, e1, e2, e3); // End Cap

    // Head Vertices (Pyramid)
    glm::vec3 h0 = -right * headThick - up * headThick + dir * shaftLen;
    glm::vec3 h1 = right * headThick - up * headThick + dir * shaftLen;
    glm::vec3 h2 = right * headThick + up * headThick + dir * shaftLen;
    glm::vec3 h3 = -right * headThick + up * headThick + dir * shaftLen;
    glm::vec3 tip = dir * (shaftLen + headLen);

    // Head Base
    addQuad(h0, h1, h2, h3);

    // Pyramid Sides (Triangles)
    auto addTri = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2) {
      uint32_t i = (uint32_t)mesh->GetVertexCount();
      mesh->AddVertex(Vertex3D(v0, glm::vec3(0), glm::vec2(0)));
      mesh->AddVertex(Vertex3D(v1, glm::vec3(0), glm::vec2(0)));
      mesh->AddVertex(Vertex3D(v2, glm::vec3(0), glm::vec2(0)));
      mesh->AddTriangle(i, i + 1, i + 2);
    };

    addTri(h0, h3, tip);
    addTri(h1, h0, tip);
    addTri(h2, h1, tip);
    addTri(h3, h2, tip);

    mesh->Finalize(device);
    return mesh;
  };

  m_AxisX = createArrow("GizmoX", glm::vec3(1, 0, 0), m_ColorX);
  m_AxisY = createArrow("GizmoY", glm::vec3(0, 1, 0), m_ColorY);
  m_AxisZ = createArrow("GizmoZ", glm::vec3(0, 0, 1), m_ColorZ);
}

void TranslateGizmo::Render(SceneRenderer *renderer, VkCommandBuffer cmd,
                            const glm::mat4 &view, const glm::mat4 &proj) {
  if (!renderer)
    return;

  SyncWithTarget();

  // Use base class scale calculation
  m_CurrentScale = CalculateScreenConstantScale();

  // Calculate Model Matrix (position + rotation + scale)
  glm::mat4 rotation = GetGizmoRotation();
  glm::mat4 model = glm::translate(glm::mat4(1.0f), m_Position);
  model = model * rotation; // Apply local/global rotation
  model = glm::scale(model, glm::vec3(m_CurrentScale));

  // Call SceneRenderer to draw each axis
  renderer->DrawGizmoMesh(cmd, m_AxisX, model, m_ColorX, view, proj);
  renderer->DrawGizmoMesh(cmd, m_AxisY, model, m_ColorY, view, proj);
  renderer->DrawGizmoMesh(cmd, m_AxisZ, model, m_ColorZ, view, proj);
}

} // namespace Quantum
