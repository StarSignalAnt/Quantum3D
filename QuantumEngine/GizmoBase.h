#pragma once
#include "Mesh3D.h"
#include "glm/glm.hpp"
#include <memory>
#include <vulkan/vulkan.h>

namespace Quantum {

class SceneRenderer; // Forward declaration
class GraphNode;     // Forward declaration

// Axis identifiers for gizmo interaction
enum class GizmoAxis { None = 0, X, Y, Z };

// Coordinate space for gizmo operations
enum class GizmoSpace { Local, Global };

// Gizmo tool type
enum class GizmoType { Translate, Rotate, Scale };

// Ray structure for picking
struct Ray {
  glm::vec3 origin{0.0f};
  glm::vec3 direction{0.0f, 0.0f, -1.0f};
};

// Result of a mesh hit test
struct MeshHitResult {
  bool hit = false;
  float distance = 0.0f;
};

class GizmoBase {
public:
  virtual ~GizmoBase() = default;

  // Returns true if the gizmo consumed the click (should block node selection)
  virtual bool OnMouseClicked(int x, int y, bool isPressed, int width,
                              int height) = 0;
  virtual void OnMouseMoved(int x, int y) = 0;
  virtual void Render(SceneRenderer *renderer, VkCommandBuffer cmd,
                      const glm::mat4 &view, const glm::mat4 &proj) = 0;

  void SetPosition(const glm::vec3 &position) { m_Position = position; }
  glm::vec3 GetPosition() const { return m_Position; }

  // Target node for manipulation
  void SetTargetNode(std::shared_ptr<GraphNode> node) { m_TargetNode = node; }
  std::shared_ptr<GraphNode> GetTargetNode() const {
    return m_TargetNode.lock();
  }

  // View state for hit detection
  void SetViewState(const glm::mat4 &view, const glm::mat4 &proj, int width,
                    int height) {
    m_ViewMatrix = view;
    m_ProjMatrix = proj;
    m_ViewportWidth = width;
    m_ViewportHeight = height;
  }

  // Check if gizmo is currently being dragged
  virtual bool IsDragging() const { return m_IsDragging; }

  // Get the currently active axis
  GizmoAxis GetActiveAxis() const { return m_ActiveAxis; }

  // Coordinate space (Local = object orientation, Global = world axes)
  void SetSpace(GizmoSpace space) { m_Space = space; }
  GizmoSpace GetSpace() const { return m_Space; }

  // Sync gizmo position with target node world position
  void SyncWithTarget();

protected:
  // Get gizmo rotation based on current space (identity for global, node
  // rotation for local)
  glm::mat4 GetGizmoRotation() const;
  // === Shared Ray Utilities ===

  // Calculate a picking ray from screen coordinates
  Ray CalculatePickingRay(int mouseX, int mouseY) const;

  // Möller-Trumbore ray-triangle intersection
  static bool RayTriangleIntersection(const Ray &ray, const glm::vec3 &v0,
                                      const glm::vec3 &v1, const glm::vec3 &v2,
                                      float &t);

  // Test a mesh for intersection with a ray
  MeshHitResult HitTestMesh(const Ray &ray, Mesh3D *mesh,
                            const glm::mat4 &modelMatrix) const;

  // Calculate scale to maintain constant screen size
  float CalculateScreenConstantScale(float baseScale = 0.15f) const;

  // Get camera position from view matrix
  glm::vec3 GetCameraPosition() const;

  // === Shared State ===
  glm::vec3 m_Position{0.0f};
  std::weak_ptr<GraphNode> m_TargetNode;
  glm::mat4 m_ViewMatrix{1.0f};
  glm::mat4 m_ProjMatrix{1.0f};
  int m_ViewportWidth = 0;
  int m_ViewportHeight = 0;

  // Drag state (shared across gizmo types)
  bool m_IsDragging = false;
  GizmoAxis m_ActiveAxis = GizmoAxis::None;
  glm::vec2 m_LastMousePos{0.0f};
  glm::vec3 m_DragStartNodePos{0.0f};
  glm::vec3 m_DragAxisDirection{0.0f}; // Axis direction captured at drag start
  float m_DragStartAxisT =
      0.0f; // Initial t-parameter along axis when drag started
  float m_CurrentScale = 1.0f;

  // Coordinate space mode
  GizmoSpace m_Space = GizmoSpace::Local;
};

} // namespace Quantum
