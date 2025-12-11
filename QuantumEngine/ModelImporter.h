#pragma once
#include "GraphNode.h"
#include "Material.h"
#include "Mesh3D.h"
#include "Texture2D.h"
#include "VividDevice.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>

// Forward declare Assimp types
struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;

namespace Quantum {

/// <summary>
/// Static class for importing 3D models using Assimp.
/// Creates GraphNode hierarchies with Mesh3D instances.
/// </summary>
class ModelImporter {
public:
  // Delete constructor - static class only
  ModelImporter() = delete;

  /// <summary>
  /// Import a 3D model file (FBX, OBJ, GLTF, etc.) as a scene graph.
  /// Creates a root GraphNode with child nodes matching the file's hierarchy.
  /// Each mesh in the file becomes a Mesh3D attached to the appropriate node.
  /// </summary>
  /// <param name="filePath">Path to the 3D model file</param>
  /// <param name="device">Vulkan device for buffer creation</param>
  /// <returns>Root node of the imported model, or nullptr on failure</returns>
  static std::shared_ptr<GraphNode> ImportEntity(const std::string &filePath,
                                                 Vivid::VividDevice *device);

  /// <summary>
  /// Set the default white texture used when textures can't be found.
  /// </summary>
  static void SetDefaultTexture(std::shared_ptr<Vivid::Texture2D> texture);

  /// <summary>
  /// Get or create the default white texture.
  /// </summary>
  static std::shared_ptr<Vivid::Texture2D>
  GetDefaultTexture(Vivid::VividDevice *device);

private:
  // Process Assimp node recursively
  static std::shared_ptr<GraphNode>
  ProcessNode(aiNode *node, const aiScene *scene, const std::string &directory,
              Vivid::VividDevice *device,
              std::unordered_map<unsigned int, std::shared_ptr<Material>>
                  &materialCache);

  // Process a single Assimp mesh
  static std::shared_ptr<Mesh3D>
  ProcessMesh(aiMesh *mesh, const aiScene *scene, const std::string &directory,
              Vivid::VividDevice *device,
              std::unordered_map<unsigned int, std::shared_ptr<Material>>
                  &materialCache);

  // Process Assimp material
  static std::shared_ptr<Material> ProcessMaterial(aiMaterial *material,
                                                   const aiScene *scene,
                                                   const std::string &directory,
                                                   Vivid::VividDevice *device);

  // Try to load texture from various paths
  static std::shared_ptr<Vivid::Texture2D>
  LoadTexture(const std::string &texturePath, const std::string &directory,
              Vivid::VividDevice *device,
              VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

  // Default white texture
  static std::shared_ptr<Vivid::Texture2D> s_DefaultTexture;
};

} // namespace Quantum
