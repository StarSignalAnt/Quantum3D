#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "GraphNode.h"
#include "Mesh3D.h"
#include <algorithm>

namespace Quantum {

GraphNode::GraphNode(const std::string &name)
    : m_Name(name), m_LocalPosition(0.0f, 0.0f, 0.0f),
      m_LocalRotation(1.0f) // Identity matrix
      ,
      m_LocalScale(1.0f, 1.0f, 1.0f), m_Parent(nullptr),
      m_CachedWorldMatrix(1.0f), m_WorldMatrixDirty(true) {}

GraphNode::~GraphNode() {
  // Remove from parent
  if (m_Parent) {
    RemoveFromParent();
  }
  // Clear children (they will handle their own cleanup)
  m_Children.clear();
}

// ========== Transform Setters ==========

void GraphNode::SetLocalPosition(const glm::vec3 &position) {
  m_LocalPosition = position;
  InvalidateTransform();
}

void GraphNode::SetLocalPosition(float x, float y, float z) {
  SetLocalPosition(glm::vec3(x, y, z));
}

void GraphNode::SetLocalRotation(const glm::mat4 &rotation) {
  m_LocalRotation = rotation;
  InvalidateTransform();
}

void GraphNode::SetLocalScale(const glm::vec3 &scale) {
  m_LocalScale = scale;
  InvalidateTransform();
}

void GraphNode::SetLocalScale(float x, float y, float z) {
  SetLocalScale(glm::vec3(x, y, z));
}

void GraphNode::SetLocalScale(float uniformScale) {
  SetLocalScale(glm::vec3(uniformScale, uniformScale, uniformScale));
}

void GraphNode::SetLocalRotationEuler(float pitch, float yaw, float roll) {
  // Create rotation matrix from Euler angles
  // Order: Yaw (Y) -> Pitch (X) -> Roll (Z) for typical camera/object behavior
  glm::mat4 rotY =
      glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
  glm::mat4 rotX =
      glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f));
  glm::mat4 rotZ =
      glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));
  m_LocalRotation = rotY * rotX * rotZ;
  InvalidateTransform();
}

void GraphNode::SetLocalRotationAxisAngle(const glm::vec3 &axis,
                                          float angleRadians) {
  m_LocalRotation =
      glm::rotate(glm::mat4(1.0f), angleRadians, glm::normalize(axis));
  InvalidateTransform();
}

void GraphNode::LookAt(const glm::vec3 &eye, const glm::vec3 &target,
                       const glm::vec3 &up) {
  // glm::lookAt returns the View Matrix (inverse of World Matrix).
  // View = inv(World)
  // World = inv(View)
  glm::mat4 viewMatrix = glm::lookAt(eye, target, up);
  glm::mat4 worldMatrix = glm::inverse(viewMatrix);

  // Extract translation (position)
  m_LocalPosition = glm::vec3(worldMatrix[3]);

  // Extract rotation (upper 3x3 of the world matrix)
  m_LocalRotation = glm::mat4(1.0f);
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      m_LocalRotation[i][j] = worldMatrix[i][j];
    }
  }

  InvalidateTransform();
}

// ========== Transform Computation ==========

glm::mat4 GraphNode::GetLocalMatrix() const {
  // For Vulkan/GLM: Model = Translation * Rotation * Scale
  // This order means: scale first, then rotate, then translate
  glm::mat4 T = glm::translate(glm::mat4(1.0f), m_LocalPosition);
  glm::mat4 S = glm::scale(glm::mat4(1.0f), m_LocalScale);

  return T * m_LocalRotation * S;
}

glm::mat4 GraphNode::GetWorldMatrix() const {
  if (m_WorldMatrixDirty) {
    glm::mat4 localMatrix = GetLocalMatrix();

    if (m_Parent) {
      // World = Parent's World * Local
      // This propagates transforms down the hierarchy
      m_CachedWorldMatrix = m_Parent->GetWorldMatrix() * localMatrix;
    } else {
      m_CachedWorldMatrix = localMatrix;
    }

    m_WorldMatrixDirty = false;
  }

  return m_CachedWorldMatrix;
}

glm::vec3 GraphNode::GetWorldPosition() const {
  glm::mat4 worldMatrix = GetWorldMatrix();
  return glm::vec3(worldMatrix[3]); // Extract translation column
}

// ========== Transform Invalidation ==========

void GraphNode::InvalidateTransform() {
  m_WorldMatrixDirty = true;
  InvalidateChildTransforms();
  OnTransformChanged();
}

void GraphNode::InvalidateChildTransforms() {
  for (auto &child : m_Children) {
    child->m_WorldMatrixDirty = true;
    child->InvalidateChildTransforms();
  }
}

void GraphNode::OnTransformChanged() {
  // Virtual hook for derived classes
}

// ========== Hierarchy Management ==========

void GraphNode::SetParent(GraphNode *parent) {
  m_Parent = parent;
  InvalidateTransform();
}

void GraphNode::AddChild(std::shared_ptr<GraphNode> child) {
  if (!child)
    return;

  // Remove from previous parent
  if (child->m_Parent) {
    child->RemoveFromParent();
  }

  child->SetParent(this);
  m_Children.push_back(child);
}

void GraphNode::RemoveChild(GraphNode *child) {
  if (!child)
    return;

  auto it = std::find_if(m_Children.begin(), m_Children.end(),
                         [child](const std::shared_ptr<GraphNode> &node) {
                           return node.get() == child;
                         });

  if (it != m_Children.end()) {
    (*it)->SetParent(nullptr);
    m_Children.erase(it);
  }
}

void GraphNode::RemoveFromParent() {
  if (m_Parent) {
    m_Parent->RemoveChild(this);
  }
}

std::shared_ptr<GraphNode> GraphNode::FindChild(const std::string &name,
                                                bool recursive) const {
  for (const auto &child : m_Children) {
    if (child->GetName() == name) {
      return child;
    }

    if (recursive) {
      auto found = child->FindChild(name, true);
      if (found) {
        return found;
      }
    }
  }
  return nullptr;
}

// ========== Mesh Management ==========

void GraphNode::AddMesh(std::shared_ptr<Mesh3D> mesh) {
  if (mesh) {
    m_Meshes.push_back(mesh);
  }
}

void GraphNode::RemoveMesh(Mesh3D *mesh) {
  if (!mesh)
    return;

  auto it = std::find_if(
      m_Meshes.begin(), m_Meshes.end(),
      [mesh](const std::shared_ptr<Mesh3D> &m) { return m.get() == mesh; });

  if (it != m_Meshes.end()) {
    m_Meshes.erase(it);
  }
}

void GraphNode::ClearMeshes() { m_Meshes.clear(); }

} // namespace Quantum
