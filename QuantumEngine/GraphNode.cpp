#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "GraphNode.h"
#include "../QLang/QClassInstance.h"
#include "Mesh3D.h"
#include "QLangDomain.h"
#include <algorithm>
#include <cmath>

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

glm::vec3 GraphNode::GetRotationEuler() const {
  // Extract Euler angles from rotation matrix (in degrees)
  // Using YXZ order: Yaw(Y) -> Pitch(X) -> Roll(Z)
  // Matrix M = Ry * Rx * Rz
  glm::vec3 euler;

  float m21 = m_LocalRotation[2][1];
  if (std::abs(m21) < 0.99999f) {
    euler.x = -std::asin(m21);                                          // pitch
    euler.y = std::atan2(m_LocalRotation[2][0], m_LocalRotation[2][2]); // yaw
    euler.z = std::atan2(m_LocalRotation[0][1], m_LocalRotation[1][1]); // roll
  } else {
    // Gimbal lock
    euler.x = m21 < 0 ? glm::half_pi<float>() : -glm::half_pi<float>();
    euler.y = std::atan2(-m_LocalRotation[0][2], m_LocalRotation[0][0]);
    euler.z = 0.0f;
  }

  return glm::degrees(euler);
}

void GraphNode::SetRotationEuler(const glm::vec3 &eulerDegrees) {
  // Convert degrees to radians
  glm::vec3 rad = glm::radians(eulerDegrees);
  SetLocalRotationEuler(rad.x, rad.y, rad.z);
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

void GraphNode::GetWorldBounds(glm::vec3 &min, glm::vec3 &max) const {
  min = glm::vec3(FLT_MAX);
  max = glm::vec3(-FLT_MAX);

  glm::mat4 worldMatrix = GetWorldMatrix();

  if (m_Meshes.empty()) {
    // If no meshes, just use position? or empty?
    // Let's use position as a point
    min = max = GetWorldPosition();
    return;
  }

  for (const auto &mesh : m_Meshes) {
    if (!mesh)
      continue;

    // Get AABB corners in local space
    glm::vec3 bMin = mesh->GetBoundsMin();
    glm::vec3 bMax = mesh->GetBoundsMax();

    // Transform all 8 corners
    std::vector<glm::vec3> corners = {
        glm::vec3(bMin.x, bMin.y, bMin.z), glm::vec3(bMax.x, bMin.y, bMin.z),
        glm::vec3(bMin.x, bMax.y, bMin.z), glm::vec3(bMax.x, bMax.y, bMin.z),
        glm::vec3(bMin.x, bMin.y, bMax.z), glm::vec3(bMax.x, bMin.y, bMax.z),
        glm::vec3(bMin.x, bMax.y, bMax.z), glm::vec3(bMax.x, bMax.y, bMax.z)};

    for (const auto &corner : corners) {
      glm::vec4 worldPos = worldMatrix * glm::vec4(corner, 1.0f);
      min = glm::min(min, glm::vec3(worldPos));
      max = glm::max(max, glm::vec3(worldPos));
    }
  }
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

void GraphNode::Turn(glm::vec3 rot) {

  float yaw = glm::radians(rot.y);
  float pitch = glm::radians(rot.x);
  float roll = glm::radians(rot.z);

  glm::mat4 rotY =
      glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
  glm::mat4 rotX =
      glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f));
  glm::mat4 rotZ =
      glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));

  glm::mat4 rotm = rotY * rotX * rotZ;

  m_LocalRotation = m_LocalRotation * rotm;

  m_WorldMatrixDirty = true;
}

void GraphNode::AddScript(std::shared_ptr<QClassInstance> cls) {

  m_QClasses.push_back(cls);
}

void GraphNode::OnPlay() {

  for (auto cls : m_QClasses) {

    // QValue result = runner.CallMethod(node1, "Update", updateArgs);
    QLangDomain::m_QLang->RunMethod(cls, "OnPlay");
  }
  int b = 5;
}

void GraphNode::OnStop() {}

void GraphNode::OnUpdate(float dt)

{

  std::vector<QValue> updateArgs = {dt};

  for (auto cls : m_QClasses) {

    // QValue result = runner.CallMethod(node1, "Update", updateArgs);
    QLangDomain::m_QLang->RunMethod(cls, "OnUpdate", updateArgs);
  }
}

std::string GraphNode::GetFullName() const {
  if (m_Parent) {
    return m_Parent->GetFullName() + "." + m_Name;
  }
  return m_Name;
}

bool GraphNode::HasScript(const std::string &className) const {
  for (const auto &cls : m_QClasses) {
    if (cls->GetQClassName() == className) {
      return true;
    }
  }
  return false;
}

} // namespace Quantum

// GraphNode::Add
