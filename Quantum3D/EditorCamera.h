#pragma once
#include <glm/glm.hpp>
#include <memory>

// Forward declarations
namespace Quantum {
class CameraNode;
}

class EditorCamera {
public:
  EditorCamera();
  ~EditorCamera() = default;

  void SetCamera(std::shared_ptr<Quantum::CameraNode> camera);

  // Rotate camera based on mouse deltas
  void Rotate(float deltaX, float deltaY);

  // Move camera based on input direction (X=Right, Y=Up, Z=Forward)
  void Move(const glm::vec3 &direction, float deltaTime);

  void SetSpeed(float speed) { m_MoveSpeed = speed; }
  void SetRotationSpeed(float speed) { m_RotationSpeed = speed; }

  // Get view matrix from underlying camera
  glm::mat4 GetViewMatrix() const;

  // Position accessors
  void SetPosition(const glm::vec3& pos);
  glm::vec3 GetPosition() const;

  // Set initial rotation state if needed
  void SetRotation(float yaw, float pitch);

  // Get current rotation state
  float GetYaw() const { return m_Yaw; }
  float GetPitch() const { return m_Pitch; }

private:
  std::shared_ptr<Quantum::CameraNode> m_Camera;

  float m_Yaw = 0.0f;
  float m_Pitch = 0.0f;

  float m_MoveSpeed = 5.0f;
  float m_RotationSpeed = 0.1f;
};
