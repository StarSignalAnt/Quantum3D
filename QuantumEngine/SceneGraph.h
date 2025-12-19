#pragma once
#include "GraphNode.h"
#include "glm/glm.hpp"
#include <functional>

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

  // Get total mesh count in the scene
  size_t GetTotalMeshCount() const;

  // Active camera
  void SetCurrentCamera(std::shared_ptr<CameraNode> camera);
  std::shared_ptr<CameraNode> GetCurrentCamera() const;

  // Lights
  void AddLight(std::shared_ptr<LightNode> light);
  const std::vector<std::shared_ptr<LightNode>> &GetLights() const {
    return m_Lights;
  }
  // Get first light position (or default if no lights)
  glm::vec3 GetLightPosition() const;

  // Ray Casting
  struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
  };

  /// <summary>
  /// Casts a ray from mouse coordinates and returns the closest intersected
  /// node.
  /// </summary>
  std::shared_ptr<GraphNode> SelectEntity(float mouseX, float mouseY, int width,
                                          int height);

  void OnPlay();
  void OnStop();
  void OnUpdate();

  // Iterate over every node in the scene graph
  void ForEveryNode(const std::function<void(GraphNode *)> &callback);

private:

   bool m_Playing = false;

  std::shared_ptr<GraphNode> m_Root;
  std::shared_ptr<CameraNode> m_CurrentCamera;
  std::vector<std::shared_ptr<LightNode>> m_Lights;

  size_t CountNodes(GraphNode *node) const;

  // Helper for recursive ray casting
  void CastRayRecursive(GraphNode *node, const Ray &ray, float &closestDistance,
                        std::shared_ptr<GraphNode> &hitNode);

  // Helper for ray-triangle intersection
  bool RayTriangleIntersection(const Ray &ray, const glm::vec3 &v0,
                               const glm::vec3 &v1, const glm::vec3 &v2,
                               float &t);

  // Helper for recursive node traversal
  void ForEveryNodeRecursive(GraphNode *node,
                             const std::function<void(GraphNode *)> &callback);
};

} // namespace Quantum
