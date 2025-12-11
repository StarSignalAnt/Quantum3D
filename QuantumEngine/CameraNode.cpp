#include "CameraNode.h"

namespace Quantum {

CameraNode::CameraNode(const std::string &name) : GraphNode(name) {}

glm::mat4 CameraNode::GetWorldMatrix() const {
  // Get the standard world matrix (transform of the camera object)
  // This represents the camera's actual position and orientation in the world.
  glm::mat4 world = GraphNode::GetWorldMatrix();

  // The View Matrix is the inverse of the Camera's World Matrix
  return glm::inverse(world);
}

glm::vec3 CameraNode::GetWorldPosition() const {
  // We need the ACTUAL world position, not extracted from view matrix.
  // So we call the base class GetWorldMatrix() directly.
  glm::mat4 worldMatrix = GraphNode::GetWorldMatrix();
  return glm::vec3(worldMatrix[3]); // Extract translation column
}

} // namespace Quantum
