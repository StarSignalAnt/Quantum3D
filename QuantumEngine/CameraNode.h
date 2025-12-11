#pragma once
#include "GraphNode.h"

namespace Quantum {

/// <summary>
/// A node representing a camera in the scene.
/// GetWorldMatrix() returns the View Matrix (inverse of world transform).
/// </summary>
class CameraNode : public GraphNode {
public:
  CameraNode(const std::string &name = "Camera");
  ~CameraNode() override = default;

  /// <summary>
  /// Overridden to return the View Matrix (inverse of the camera's world
  /// transform).
  /// </summary>
  glm::mat4 GetWorldMatrix() const override;

  /// <summary>
  /// Overridden to return the actual world position of the camera.
  /// (GetWorldMatrix returns view matrix, so we must use base class method)
  /// </summary>
  glm::vec3 GetWorldPosition() const override;
};

} // namespace Quantum
