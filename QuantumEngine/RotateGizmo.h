#pragma once
#include "GizmoBase.h"
#include "Mesh3D.h"
#include "VividDevice.h"
#include "glm/gtc/quaternion.hpp"
#include <memory>

namespace Quantum {

class RotateGizmo : public GizmoBase {
public:
  RotateGizmo(Vivid::VividDevice *device);
  ~RotateGizmo() override = default;

  bool OnMouseClicked(int x, int y, bool isPressed, int width,
                      int height) override;
  void OnMouseMoved(int x, int y) override;
  void Render(SceneRenderer *renderer, VkCommandBuffer cmd,
              const glm::mat4 &view, const glm::mat4 &proj) override;

private:
  void GenerateMeshes(Vivid::VividDevice *device);
  GizmoAxis HitTest(int mouseX, int mouseY, int width, int height);

  // Get rotation axis for the given gizmo axis
  glm::vec3 GetRotationAxis(GizmoAxis axis) const;

  // Calculate angle from mouse position relative to gizmo center
  float CalculateAngleFromMouse(int mouseX, int mouseY, GizmoAxis axis);

  // Ring meshes for each axis
  std::shared_ptr<Mesh3D> m_RingX;
  std::shared_ptr<Mesh3D> m_RingY;
  std::shared_ptr<Mesh3D> m_RingZ;

  // Default tint colors for axes
  glm::vec3 m_ColorX{1.0f, 0.0f, 0.0f}; // Red
  glm::vec3 m_ColorY{0.0f, 1.0f, 0.0f}; // Green
  glm::vec3 m_ColorZ{0.0f, 0.0f, 1.0f}; // Blue

  // Rotation state
  float m_DragStartAngle = 0.0f;
  glm::quat m_DragStartRotation{1.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace Quantum
