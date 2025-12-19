#include "SceneGraph.h"
#include "CameraNode.h"
#include "LightNode.h"
#include "Mesh3D.h"
#include "glm/gtc/matrix_transform.hpp"
#include "pch.h"
#include <algorithm>
#include <functional>
#include <limits>

namespace Quantum {

SceneGraph::SceneGraph() { m_Root = std::make_shared<GraphNode>("Root"); }

SceneGraph::~SceneGraph() { Clear(); }

std::shared_ptr<GraphNode> SceneGraph::CreateNode(const std::string &name,
                                                  GraphNode *parent) {
  auto node = std::make_shared<GraphNode>(name);

  if (parent) {
    parent->AddChild(node);
  } else {
    // Add to root by default
    m_Root->AddChild(node);
  }

  return node;
}

std::shared_ptr<GraphNode> SceneGraph::FindNode(const std::string &name) const {
  if (m_Root->GetName() == name) {
    return m_Root;
  }
  return m_Root->FindChild(name, true);
}

void SceneGraph::Clear() {
  // Remove all children from root
  auto &children = m_Root->GetChildren();
  while (!children.empty()) {
    m_Root->RemoveChild(children.front().get());
  }
  m_Lights.clear();
}

void SceneGraph::AddLight(std::shared_ptr<LightNode> light) {
  if (light) {
    // Add to scene hierarchy (attach to root if no parent)
    if (!light->GetParent()) {
      m_Root->AddChild(light);
    }
    // Add to lights list
    m_Lights.push_back(light);
  }
}

size_t SceneGraph::GetNodeCount() const { return CountNodes(m_Root.get()); }

void SceneGraph::SetCurrentCamera(std::shared_ptr<CameraNode> camera) {
  m_CurrentCamera = camera;
}

std::shared_ptr<CameraNode> SceneGraph::GetCurrentCamera() const {
  return m_CurrentCamera;
}

size_t SceneGraph::CountNodes(GraphNode *node) const {
  if (!node)
    return 0;

  size_t count = 1; // Count this node
  for (const auto &child : node->GetChildren()) {
    count += CountNodes(child.get());
  }
  return count;
}

glm::vec3 SceneGraph::GetLightPosition() const {
  if (!m_Lights.empty() && m_Lights[0]) {
    return m_Lights[0]->GetWorldPosition();
  }
  // Default light position if no lights in scene
  return glm::vec3(5.0f, 5.0f, 5.0f);
}

size_t SceneGraph::GetTotalMeshCount() const {
  size_t count = 0;
  // Simple recursive helper (lambda not needed if we iterate directly,
  // but recursion handles depth)
  std::function<void(GraphNode *)> traverse = [&](GraphNode *node) {
    if (!node)
      return;
    count += node->GetMeshCount();
    for (const auto &child : node->GetChildren()) {
      traverse(child.get());
    }
  };
  traverse(m_Root.get());
  return count;
}

// =================================================================================================
// Ray Casting / Picking
// =================================================================================================

std::shared_ptr<GraphNode> SceneGraph::SelectEntity(float mouseX, float mouseY,
                                                    int width, int height) {
  if (!m_CurrentCamera || !m_Root)
    return nullptr;

  // 1. Calculate Normalized Device Coordinates (NDC)
  float x = (2.0f * mouseX) / width - 1.0f;
  float y = (2.0f * mouseY) / height - 1.0f;

  glm::vec4 ray_clip = glm::vec4(x, y, -1.0, 1.0);

  // 2. Unproject to View Space
  glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                    (float)width / (float)height, 0.1f, 100.0f);
  proj[1][1] *= -1; // Match SceneRenderer Y-flip

  glm::vec4 ray_eye = glm::inverse(proj) * ray_clip;
  ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0); // Forward is -Z

  // 3. Unproject to World Space
  glm::mat4 view =
      m_CurrentCamera->GetWorldMatrix();  // CameraNode returns View Matrix
  glm::mat4 invView = glm::inverse(view); // Camera World Transform

  glm::vec3 ray_wor = glm::vec3(invView * ray_eye);
  ray_wor = glm::normalize(ray_wor);

  Ray ray;
  ray.origin = m_CurrentCamera->GetWorldPosition();
  ray.direction = ray_wor;

  // 4. Cast Ray
  float closestDistance = std::numeric_limits<float>::max();
  std::shared_ptr<GraphNode> hitNode = nullptr;

  CastRayRecursive(m_Root.get(), ray, closestDistance, hitNode);

  return hitNode;
}

void SceneGraph::CastRayRecursive(GraphNode *node, const Ray &ray,
                                  float &closestDistance,
                                  std::shared_ptr<GraphNode> &hitNode) {
  if (!node)
    return;

  for (const auto &child : node->GetChildren()) {
    if (!child)
      continue;

    glm::mat4 model = child->GetWorldMatrix();
    const auto &meshes = child->GetMeshes();

    // Check meshes
    for (const auto &mesh : meshes) {
      if (!mesh)
        continue;
      const auto &vertices = mesh->GetVertices();
      const auto &triangles = mesh->GetTriangles();

      for (const auto &tri : triangles) {
        glm::vec3 v0 =
            glm::vec3(model * glm::vec4(vertices[tri.v0].position, 1.0f));
        glm::vec3 v1 =
            glm::vec3(model * glm::vec4(vertices[tri.v1].position, 1.0f));
        glm::vec3 v2 =
            glm::vec3(model * glm::vec4(vertices[tri.v2].position, 1.0f));

        float t = 0.0f;
        if (RayTriangleIntersection(ray, v0, v1, v2, t)) {
          if (t > 0.0f && t < closestDistance) {
            closestDistance = t;
            hitNode = child;
          }
        }
      }
    }

    CastRayRecursive(child.get(), ray, closestDistance, hitNode);
  }
}

bool SceneGraph::RayTriangleIntersection(const Ray &ray, const glm::vec3 &v0,
                                         const glm::vec3 &v1,
                                         const glm::vec3 &v2, float &t) {
  const float EPSILON = 0.0000001f;
  glm::vec3 edge1, edge2, h, s, q;
  float a, f, u, v;

  edge1 = v1 - v0;
  edge2 = v2 - v0;
  h = glm::cross(ray.direction, edge2);
  a = glm::dot(edge1, h);

  if (a > -EPSILON && a < EPSILON)
    return false;

  f = 1.0f / a;
  s = ray.origin - v0;
  u = f * glm::dot(s, h);

  if (u < 0.0f || u > 1.0f)
    return false;

  q = glm::cross(s, edge1);
  v = f * glm::dot(ray.direction, q);

  if (v < 0.0f || u + v > 1.0f)
    return false;

  t = f * glm::dot(edge2, q);

  if (t > EPSILON)
    return true;
  else
    return false;
}

void SceneGraph::OnPlay() {

    if (m_Playing) return;
    m_Playing = true;

    ForEveryNode([](Quantum::GraphNode* node) {
        node->OnPlay();
        // std::cout << "Node: " << node->GetName() << std::endl;
        // Do something with the node
        });

}

void SceneGraph::OnStop()
{
    if (!m_Playing) return;
    ForEveryNode([](Quantum::GraphNode* node) {
    //    std::cout << "Node: " << node->GetName() << std::endl;
        // Do something with the node
        node->OnStop();
        });
    m_Playing = false;
}

void SceneGraph::OnUpdate() {

    if (!m_Playing) return;

    ForEveryNode([](Quantum::GraphNode* node) {
       // std::cout << "Node: " << node->GetName() << std::endl;
        // Do something with the node
        node->OnUpdate();
        });


}

void SceneGraph::ForEveryNode(
    const std::function<void(GraphNode *)> &callback) {
  if (m_Root) {
    ForEveryNodeRecursive(m_Root.get(), callback);
  }
}

void SceneGraph::ForEveryNodeRecursive(
    GraphNode *node, const std::function<void(GraphNode *)> &callback) {
  if (!node)
    return;

  // Call the callback on this node
  callback(node);

  // Recurse into children
  for (const auto &child : node->GetChildren()) {
    ForEveryNodeRecursive(child.get(), callback);
  }
}

} // namespace Quantum
