#pragma once
#include "GraphNode.h"

namespace Quantum {

// Forward declaration
class Mesh3D;
class CameraNode;
class LightNode;

/// <summary>
/// Manages a hierarchical scene graph of GraphNodes.
/// Contains the root node which serves as the scene's origin.
/// </summary>
class SceneGraph {
public:
  SceneGraph();
  ~SceneGraph();

  // Get the root node (scene origin)
  GraphNode *GetRoot() const { return m_Root.get(); }

  // Create a new node and add it to the scene
  std::shared_ptr<GraphNode> CreateNode(const std::string &name,
                                        GraphNode *parent = nullptr);

  // Find a node by name (searches entire tree)
  std::shared_ptr<GraphNode> FindNode(const std::string &name) const;

  // Clear all nodes except root
  void Clear();

  // Get total node count (including root)
  size_t GetNodeCount() const;

  // Active camera
  void SetCurrentCamera(std::shared_ptr<CameraNode> camera);
  std::shared_ptr<CameraNode> GetCurrentCamera() const;

  // Lights
  void AddLight(std::shared_ptr<LightNode> light);
  const std::vector<std::shared_ptr<LightNode>> &GetLights() const {
    return m_Lights;
  }

private:
  std::shared_ptr<GraphNode> m_Root;
  std::shared_ptr<CameraNode> m_CurrentCamera;
  std::vector<std::shared_ptr<LightNode>> m_Lights;

  size_t CountNodes(GraphNode *node) const;
};

} // namespace Quantum
