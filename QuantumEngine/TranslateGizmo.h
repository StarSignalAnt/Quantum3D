#pragma once
#include "GizmoBase.h"
#include "Mesh3D.h"
#include "VividDevice.h"
#include <memory>
#include <vector>

namespace Quantum {

class TranslateGizmo : public GizmoBase {
public:
  TranslateGizmo(Vivid::VividDevice *device);
  ~TranslateGizmo() override = default;

  bool OnMouseClicked(int x, int y, bool isPressed, int width,
                      int height) override;
  void OnMouseMoved(int x, int y) override;
  void Render(SceneRenderer *renderer, VkCommandBuffer cmd,
              const glm::mat4 &view, const glm::mat4 &proj) override;

private:
  void GenerateMeshes(Vivid::VividDevice *device);
  GizmoAxis HitTest(int mouseX, int mouseY, int width, int height);
  glm::vec3 GetAxisDirection(GizmoAxis axis) const;
  glm::vec3 ProjectMouseToAxis(int mouseX, int mouseY, GizmoAxis axis);

  std::shared_ptr<Mesh3D> m_AxisX;
  std::shared_ptr<Mesh3D> m_AxisY;
  std::shared_ptr<Mesh3D> m_AxisZ;

  // Default tint colors for axes
  glm::vec3 m_ColorX{1.0f, 0.0f, 0.0f}; // Red
  glm::vec3 m_ColorY{0.0f, 1.0f, 0.0f}; // Green
  glm::vec3 m_ColorZ{0.0f, 0.0f, 1.0f}; // Blue
};

} // namespace Quantum
