#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declaration for QLang (global namespace)
class QClassInstance;

namespace Quantum {

// Forward declaration
class Mesh3D;

/// <summary>
/// A node in the scene graph hierarchy with transform properties.
/// Supports parent-child relationships and computes world transforms.
/// </summary>
class GraphNode {
public:
  GraphNode(const std::string &name = "Node");
  virtual ~GraphNode();

  // Name
  const std::string &GetName() const { return m_Name; }
  void SetName(const std::string &name) { m_Name = name; }

  // Transform getters
  const glm::vec3 &GetLocalPosition() const { return m_LocalPosition; }
  const glm::mat4 &GetLocalRotation() const { return m_LocalRotation; }
  const glm::vec3 &GetLocalScale() const { return m_LocalScale; }

  // Transform setters
  void SetLocalPosition(const glm::vec3 &position);
  void SetLocalPosition(float x, float y, float z);
  void SetLocalRotation(const glm::mat4 &rotation);
  void SetLocalScale(const glm::vec3 &scale);
  void SetLocalScale(float x, float y, float z);
  void SetLocalScale(float uniformScale);

  // Convenience rotation setters
  void SetLocalRotationEuler(float pitch, float yaw, float roll);
  void SetLocalRotationAxisAngle(const glm::vec3 &axis, float angleRadians);

  // Euler angle rotation (in degrees) - for scripting
  glm::vec3 GetRotationEuler() const;
  void SetRotationEuler(const glm::vec3 &eulerDegrees);

  // Orients the node to look at a target position from a specific world
  // position. Sets both LocalPosition and LocalRotation.
  void LookAt(const glm::vec3 &eye, const glm::vec3 &target,
              const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f));

  // Get the local transform matrix (T * R * S for Vulkan/GLM)
  glm::mat4 GetLocalMatrix() const;

  // Get the world transform matrix (includes parent transforms)
  virtual glm::mat4 GetWorldMatrix() const;

  // Get world position (extracted from world matrix)
  virtual glm::vec3 GetWorldPosition() const;

  // Get world bounds of this node (mesh only)
  void GetWorldBounds(glm::vec3 &min, glm::vec3 &max) const;

  // Hierarchy
  GraphNode *GetParent() const { return m_Parent; }
  const std::vector<std::shared_ptr<GraphNode>> &GetChildren() const {
    return m_Children;
  }

  void AddChild(std::shared_ptr<GraphNode> child);
  void RemoveChild(GraphNode *child);
  void RemoveFromParent();
  std::shared_ptr<GraphNode> FindChild(const std::string &name,
                                       bool recursive = false) const;

  // Mark transform as dirty (forces recalculation)
  void InvalidateTransform();

  // Meshes (one per material typically)
  void AddMesh(std::shared_ptr<Mesh3D> mesh);
  void RemoveMesh(Mesh3D *mesh);
  void ClearMeshes();
  const std::vector<std::shared_ptr<Mesh3D>> &GetMeshes() const {
    return m_Meshes;
  }
  size_t GetMeshCount() const { return m_Meshes.size(); }
  bool HasMeshes() const { return !m_Meshes.empty(); }

  void AddScript(std::shared_ptr<QClassInstance> cls);

  void Turn(glm::vec3 value);

  void OnPlay();
  void OnStop();
  void OnUpdate(float dt);

protected:
  // Called when transform changes
  virtual void OnTransformChanged();

private:
  std::string m_Name;

  // Local transform components
  glm::vec3 m_LocalPosition;
  glm::mat4 m_LocalRotation; // Rotation as matrix for flexibility
  glm::vec3 m_LocalScale;

  // Hierarchy
  GraphNode *m_Parent;
  std::vector<std::shared_ptr<GraphNode>> m_Children;

  // Cached world matrix
  mutable glm::mat4 m_CachedWorldMatrix;
  mutable bool m_WorldMatrixDirty;

  // Meshes attached to this node
  std::vector<std::shared_ptr<Mesh3D>> m_Meshes;

  // scripts

  std::vector<std::shared_ptr<QClassInstance>> m_QClasses;

  void SetParent(GraphNode *parent);
  void InvalidateChildTransforms();
};

} // namespace Quantum
