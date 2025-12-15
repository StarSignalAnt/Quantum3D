#include "SceneGraph.h"
#include "LightNode.h"

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

} // namespace Quantum
