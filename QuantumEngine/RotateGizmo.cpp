#include "RotateGizmo.h"
#include "GraphNode.h"
#include "RenderingPipelines.h"
#include "SceneRenderer.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include <cmath>
#include <iostream>

namespace Quantum {

RotateGizmo::RotateGizmo(Vivid::VividDevice *device) { GenerateMeshes(device); }

void RotateGizmo::GenerateMeshes(Vivid::VividDevice *device) {
  // Generate torus/ring geometry for each axis
  // A torus is defined by major radius (R) and minor radius (r)
  const float majorRadius = 1.0f;  // Distance from center to tube center
  const float minorRadius = 0.03f; // Tube thickness
  const int majorSegments = 48;    // Segments around the ring
  const int minorSegments = 12;    // Segments around the tube

  auto createTorus = [&](glm::vec3 rotationAxis,
                         float rotationAngle) -> std::shared_ptr<Mesh3D> {
    std::vector<Vertex3D> vertices;
    std::vector<Triangle> triangles;

    glm::mat4 rotation =
        glm::rotate(glm::mat4(1.0f), glm::radians(rotationAngle), rotationAxis);

    for (int i = 0; i <= majorSegments; ++i) {
      float u = (float)i / majorSegments * 2.0f * glm::pi<float>();

      for (int j = 0; j <= minorSegments; ++j) {
        float v = (float)j / minorSegments * 2.0f * glm::pi<float>();

        // Torus parametric equations (in XY plane by default)
        float x = (majorRadius + minorRadius * cos(v)) * cos(u);
        float y = (majorRadius + minorRadius * cos(v)) * sin(u);
        float z = minorRadius * sin(v);

        // Normal
        float nx = cos(v) * cos(u);
        float ny = cos(v) * sin(u);
        float nz = sin(v);

        // Apply rotation to orient ring
        glm::vec4 pos = rotation * glm::vec4(x, y, z, 1.0f);
        glm::vec4 norm = rotation * glm::vec4(nx, ny, nz, 0.0f);

        Vertex3D vertex{};
        vertex.position = glm::vec3(pos);
        vertex.normal = glm::normalize(glm::vec3(norm));
        vertex.uv =
            glm::vec2((float)i / majorSegments, (float)j / minorSegments);
        vertices.push_back(vertex);
      }
    }

    // Generate triangles
    for (int i = 0; i < majorSegments; ++i) {
      for (int j = 0; j < minorSegments; ++j) {
        uint32_t a = i * (minorSegments + 1) + j;
        uint32_t b = a + minorSegments + 1;

        triangles.push_back(Triangle(a, b, a + 1));
        triangles.push_back(Triangle(b, b + 1, a + 1));
      }
    }

    auto mesh = std::make_shared<Mesh3D>();
    mesh->SetVertices(vertices);
    mesh->SetTriangles(triangles);
    mesh->Finalize(device);
    return mesh;
  };

  // X ring: rotate 90 degrees around Y to make ring perpendicular to X axis
  m_RingX = createTorus(glm::vec3(0, 1, 0), 90.0f);

  // Y ring: rotate 90 degrees around X to make ring perpendicular to Y axis
  m_RingY = createTorus(glm::vec3(1, 0, 0), 90.0f);

  // Z ring: no rotation needed, torus is already in XY plane perpendicular to Z
  m_RingZ = createTorus(glm::vec3(0, 0, 1), 0.0f);
}

bool RotateGizmo::OnMouseClicked(int x, int y, bool isPressed, int width,
                                 int height) {
  if (isPressed) {
    GizmoAxis hit = HitTest(x, y, width, height);
    if (hit != GizmoAxis::None) {
      m_IsDragging = true;
      m_ActiveAxis = hit;
      m_LastMousePos = glm::vec2(x, y);

      auto target = GetTargetNode();
      if (target) {
        m_DragStartNodePos = target->GetWorldPosition();
        m_DragAxisDirection = GetRotationAxis(m_ActiveAxis);
        m_DragStartAngle = CalculateAngleFromMouse(x, y, m_ActiveAxis);

        // Store starting rotation
        // GraphNode stores rotation as mat4, convert to quaternion
        glm::mat4 rotMat = target->GetLocalRotation();
        m_DragStartRotation = glm::quat_cast(rotMat);
      }

      return true;
    }
  } else {
    if (m_IsDragging) {
      m_IsDragging = false;
      m_ActiveAxis = GizmoAxis::None;
      return true;
    }
  }
  return false;
}

void RotateGizmo::OnMouseMoved(int x, int y) {
  if (!m_IsDragging || m_ActiveAxis == GizmoAxis::None)
    return;

  auto target = GetTargetNode();
  if (!target)
    return;

  // Calculate current angle
  float currentAngle = CalculateAngleFromMouse(x, y, m_ActiveAxis);
  float deltaAngle = currentAngle - m_DragStartAngle;

  // Create rotation quaternion for the delta
  glm::vec3 axis = m_DragAxisDirection;
  glm::quat deltaRotation = glm::angleAxis(deltaAngle, axis);

  // Apply rotation to starting rotation
  glm::quat newRotation = deltaRotation * m_DragStartRotation;

  // Convert quaternion back to rotation matrix and set on node
  glm::mat4 newRotMat = glm::mat4_cast(newRotation);
  target->SetLocalRotation(newRotMat);
}

GizmoAxis RotateGizmo::HitTest(int mouseX, int mouseY, int width, int height) {
  // Calculate gizmo model matrix
  float scale = m_CurrentScale;
  if (scale < 0.001f) {
    scale = CalculateScreenConstantScale();
  }

  glm::mat4 rotation = GetGizmoRotation();
  glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), m_Position);
  modelMatrix = modelMatrix * rotation;
  modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));

  // Create picking ray
  Ray ray = CalculatePickingRay(mouseX, mouseY);

  float bestDist = std::numeric_limits<float>::max();
  GizmoAxis bestAxis = GizmoAxis::None;

  // Test each ring - use mesh intersection
  auto testRing = [&](std::shared_ptr<Mesh3D> ring, GizmoAxis axis) {
    MeshHitResult result = HitTestMesh(ray, ring.get(), modelMatrix);
    if (result.hit && result.distance < bestDist) {
      bestDist = result.distance;
      bestAxis = axis;
    }
  };

  testRing(m_RingX, GizmoAxis::X);
  testRing(m_RingY, GizmoAxis::Y);
  testRing(m_RingZ, GizmoAxis::Z);

  return bestAxis;
}

glm::vec3 RotateGizmo::GetRotationAxis(GizmoAxis axis) const {
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

  // Transform by current gizmo rotation (for local/global space)
  glm::mat4 rot = GetGizmoRotation();
  return glm::normalize(glm::vec3(rot * glm::vec4(baseAxis, 0.0f)));
}

float RotateGizmo::CalculateAngleFromMouse(int mouseX, int mouseY,
                                           GizmoAxis axis) {
  // Get the rotation axis in world space
  glm::vec3 rotAxis = m_DragAxisDirection;

  // Get camera view direction
  glm::vec3 camPos = GetCameraPosition();
  glm::vec3 viewDir = glm::normalize(m_Position - camPos);

  // Create a coordinate system on the rotation plane
  // The plane is perpendicular to rotAxis, passing through gizmo center
  // We need two orthogonal vectors in this plane
  glm::vec3 planeU, planeV;

  // Choose an up vector that's not parallel to rotAxis
  glm::vec3 up = (std::abs(glm::dot(rotAxis, glm::vec3(0, 1, 0))) < 0.99f)
                     ? glm::vec3(0, 1, 0)
                     : glm::vec3(1, 0, 0);

  planeU = glm::normalize(glm::cross(up, rotAxis));
  planeV = glm::normalize(glm::cross(rotAxis, planeU));

  // Project screen coordinates to the rotation plane at gizmo location
  // Cast a ray from camera through mouse position
  float ndcX = (2.0f * mouseX / m_ViewportWidth) - 1.0f;
  float ndcY = 1.0f - (2.0f * mouseY / m_ViewportHeight); // Flip Y for Vulkan

  // Unproject to world space
  glm::mat4 invViewProj = glm::inverse(m_ProjMatrix * m_ViewMatrix);
  glm::vec4 nearPoint4 = invViewProj * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
  glm::vec4 farPoint4 = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

  glm::vec3 nearPoint = glm::vec3(nearPoint4) / nearPoint4.w;
  glm::vec3 farPoint = glm::vec3(farPoint4) / farPoint4.w;
  glm::vec3 rayDir = glm::normalize(farPoint - nearPoint);

  // Intersect ray with the rotation plane
  // Plane equation: dot(P - gizmoPos, rotAxis) = 0
  float denom = glm::dot(rayDir, rotAxis);
  if (std::abs(denom) < 0.0001f) {
    // Ray nearly parallel to plane, use simple screen angle as fallback
    glm::mat4 viewProj = m_ProjMatrix * m_ViewMatrix;
    glm::vec4 centerClip = viewProj * glm::vec4(m_Position, 1.0f);
    if (centerClip.w > 0.0f) {
      glm::vec2 centerScreen;
      centerScreen.x =
          ((centerClip.x / centerClip.w) * 0.5f + 0.5f) * m_ViewportWidth;
      centerScreen.y = (1.0f - ((centerClip.y / centerClip.w) * 0.5f + 0.5f)) *
                       m_ViewportHeight;
      glm::vec2 dir = glm::vec2(mouseX, mouseY) - centerScreen;
      return atan2(dir.y, dir.x);
    }
    return 0.0f;
  }

  float t = glm::dot(m_Position - nearPoint, rotAxis) / denom;
  glm::vec3 planeIntersect = nearPoint + rayDir * t;

  // Get direction from gizmo center to intersection point
  glm::vec3 toMouse = planeIntersect - m_Position;

  // Project onto the plane's coordinate system
  float u = glm::dot(toMouse, planeU);
  float v = glm::dot(toMouse, planeV);

  // Calculate angle
  float angle = atan2(v, u);

  // Flip the angle if the camera is on the "back side" of the rotation plane
  // This makes clockwise mouse motion always result in positive rotation
  if (glm::dot(viewDir, rotAxis) > 0.0f) {
    angle = -angle;
  }

  return angle;
}

void RotateGizmo::Render(SceneRenderer *renderer, VkCommandBuffer cmd,
                         const glm::mat4 &view, const glm::mat4 &proj) {
  if (!m_RingX || !m_RingY || !m_RingZ)
    return;

  // Update scale for screen-constant size
  m_CurrentScale = CalculateScreenConstantScale();

  // Calculate Model Matrix
  glm::mat4 rotation = GetGizmoRotation();
  glm::mat4 model = glm::translate(glm::mat4(1.0f), m_Position);
  model = model * rotation;
  model = glm::scale(model, glm::vec3(m_CurrentScale));

  // Draw each ring with highlight if active
  glm::vec3 colorX = (m_ActiveAxis == GizmoAxis::X && m_IsDragging)
                         ? glm::vec3(1.0f, 1.0f, 0.0f)
                         : m_ColorX;
  glm::vec3 colorY = (m_ActiveAxis == GizmoAxis::Y && m_IsDragging)
                         ? glm::vec3(1.0f, 1.0f, 0.0f)
                         : m_ColorY;
  glm::vec3 colorZ = (m_ActiveAxis == GizmoAxis::Z && m_IsDragging)
                         ? glm::vec3(1.0f, 1.0f, 0.0f)
                         : m_ColorZ;

  renderer->DrawGizmoMesh(cmd, m_RingX, model, colorX, view, proj);
  renderer->DrawGizmoMesh(cmd, m_RingY, model, colorY, view, proj);
  renderer->DrawGizmoMesh(cmd, m_RingZ, model, colorZ, view, proj);
}

} // namespace Quantum
