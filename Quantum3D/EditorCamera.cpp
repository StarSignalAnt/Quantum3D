#include "EditorCamera.h"
#include "../QuantumEngine/CameraNode.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>


EditorCamera::EditorCamera() {}

void EditorCamera::SetCamera(std::shared_ptr<Quantum::CameraNode> camera) {
  m_Camera = camera;
}

void EditorCamera::SetRotation(float yaw, float pitch) {
  m_Yaw = yaw;
  m_Pitch = pitch;
}

void EditorCamera::Rotate(float deltaX, float deltaY) {
  if (!m_Camera)
    return;

  // Update yaw/pitch
  m_Yaw -= deltaX * m_RotationSpeed * 0.01f;
  m_Pitch -= deltaY * m_RotationSpeed * 0.01f;

  // Clamp pitch to avoid flipping over
  m_Pitch = std::max(-1.5f, std::min(1.5f, m_Pitch));

  // Construct rotation matrix
  // Order: Yaw (Y) -> Pitch (X) -> Roll (Z)
  glm::mat4 rotY =
      glm::rotate(glm::mat4(1.0f), m_Yaw, glm::vec3(0.0f, 1.0f, 0.0f));
  glm::mat4 rotX =
      glm::rotate(glm::mat4(1.0f), m_Pitch, glm::vec3(1.0f, 0.0f, 0.0f));

  m_Camera->SetLocalRotation(rotY * rotX);
}

void EditorCamera::Move(const glm::vec3 &inputDirection, float deltaTime) {
  if (!m_Camera)
    return;

  // Only move if there is input
  if (glm::length(inputDirection) < 0.001f)
    return;

  // Get camera's current orientation vectors
  glm::mat4 camRot = m_Camera->GetLocalRotation();

  // In our system:
  // Forward is -Z (local)
  // Right is +X (local)
  // Up is +Y (global logic usually prefers global up for consistency, but local
  // up for flying) We'll use local Forward/Right, and Global Up for E/Q for
  // standard editor feel.

  glm::vec3 forward = -glm::normalize(glm::vec3(camRot[2]));
  glm::vec3 right = glm::normalize(glm::vec3(camRot[0]));
  glm::vec3 globalUp = glm::vec3(0.0f, 1.0f, 0.0f);

  glm::vec3 finalMoveDir(0.0f);

  // Apply input mapping:
  // input.z = Forward/Backward
  finalMoveDir += forward * inputDirection.z;

  // input.x = Right/Left
  finalMoveDir += right * inputDirection.x;

  // input.y = Up/Down
  finalMoveDir += globalUp * inputDirection.y;

  if (glm::length(finalMoveDir) > 0.001f) {
    finalMoveDir = glm::normalize(finalMoveDir);
    glm::vec3 currentPos = m_Camera->GetLocalPosition();
    m_Camera->SetLocalPosition(currentPos +
                               finalMoveDir * m_MoveSpeed * deltaTime);
  }
}
