#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "ModelImporter.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <iostream>

namespace Quantum {

// Static member initialization
std::shared_ptr<Vivid::Texture2D> ModelImporter::s_DefaultTexture = nullptr;

std::shared_ptr<GraphNode>
ModelImporter::ImportEntity(const std::string &filePath,
                            Vivid::VividDevice *device) {
  Assimp::Importer importer;

  // Import flags for optimal mesh processing
  std::cout << "[ModelImporter] ImportEntity called for " << filePath
            << " with Device: " << (void *)device << std::endl;

  unsigned int flags =
      aiProcess_Triangulate |           // Ensure triangles only
      aiProcess_GenNormals |            // Generate normals if missing
      aiProcess_CalcTangentSpace |      // Calculate tangents for normal mapping
      aiProcess_FlipUVs |               // Flip UV coordinates for Vulkan
      aiProcess_JoinIdenticalVertices | // Optimize vertex count
      aiProcess_OptimizeMeshes;         // Reduce draw calls

  const aiScene *scene = importer.ReadFile(filePath, flags);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
      !scene->mRootNode) {
    std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
    return nullptr;
  }

  // Get directory for texture loading
  std::filesystem::path path(filePath);
  std::string directory = path.parent_path().string();

  // Material cache to avoid duplicates
  std::unordered_map<unsigned int, std::shared_ptr<Material>> materialCache;

  // Process the root node recursively
  auto rootNode =
      ProcessNode(scene->mRootNode, scene, directory, device, materialCache);

  if (rootNode) {
    // Set name from filename if root has default name
    if (rootNode->GetName() == "Node" || rootNode->GetName().empty()) {
      rootNode->SetName(path.stem().string());
    }
  }

  return rootNode;
}

std::shared_ptr<GraphNode> ModelImporter::ProcessNode(
    aiNode *node, const aiScene *scene, const std::string &directory,
    Vivid::VividDevice *device,
    std::unordered_map<unsigned int, std::shared_ptr<Material>>
        &materialCache) {
  auto graphNode = std::make_shared<GraphNode>(node->mName.C_Str());

  // Extract transform from Assimp matrix
  aiMatrix4x4 m = node->mTransformation;

  // Assimp uses row-major, GLM uses column-major - transpose
  glm::mat4 transform(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3,
                      m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);

  // Coordinate system conversion (Blender Z-up -> Vulkan Y-up)
  // Vertex transformation in ProcessMesh uses: x, z, -y
  // This corresponds to a rotation of -90 degrees around the X axis.
  // We must apply the same basis change to the node transform:
  // T_new = B * T_old * B^-1

  static const glm::mat4 correction = glm::rotate(
      glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
  static const glm::mat4 invCorrection = glm::inverse(correction);

  glm::mat4 newTransform = correction * transform * invCorrection;

  // Decompose transform using GLM to properly handle scaling
  glm::vec3 scale;
  glm::quat rotation;
  glm::vec3 translation;
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::decompose(newTransform, scale, rotation, translation, skew, perspective);

  std::cout << "Node Pos:" << translation.x << "Y:" << translation.y << " Z:" << translation.z << std::endl;
  
  translation = translation / scale;

  float ty = translation.y;
  translation.y = -translation.z;
  translation.z = ty;

  graphNode->SetLocalPosition(translation);
  // 
  //graphNode->SetLocalRotation(glm::mat4_cast(rotation));
  graphNode->SetLocalScale(glm::vec3(1, 1, 1));



  // Debug print to confirm scale
  if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
    std::cout << "[ModelImporter] Node '" << node->mName.C_Str()
              << "' has scale: " << scale.x << ", " << scale.y << ", "
              << scale.z << std::endl;
  }

  // Process all meshes in this node
  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
    auto mesh3D = ProcessMesh(mesh, scene, directory, device, materialCache);
    if (mesh3D) {
      graphNode->AddMesh(mesh3D);
    }
  }

  // Process children recursively
  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    auto childNode = ProcessNode(node->mChildren[i], scene, directory, device,
                                 materialCache);
    if (childNode) {
      graphNode->AddChild(childNode);
    }
  }

  return graphNode;
}

std::shared_ptr<Mesh3D> ModelImporter::ProcessMesh(
    aiMesh *mesh, const aiScene *scene, const std::string &directory,
    Vivid::VividDevice *device,
    std::unordered_map<unsigned int, std::shared_ptr<Material>>
        &materialCache) {
  auto mesh3D = std::make_shared<Mesh3D>(mesh->mName.C_Str());

  // Process vertices
  for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
    Vertex3D vertex;

    // Position - convert from Blender Z-up to Y-up coordinate system
    // Blender: X right, Y forward, Z up
    // Vulkan:  X right, Y up, Z forward (into screen)
    vertex.position = glm::vec3(mesh->mVertices[i].x,
                                mesh->mVertices[i].z,   // Z becomes Y
                                -mesh->mVertices[i].y); // -Y becomes Z

    // Normal - same coordinate conversion
    if (mesh->HasNormals()) {
      vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].z,
                                -mesh->mNormals[i].y);
    }

    // UV (first set only)
    if (mesh->mTextureCoords[0]) {
      vertex.uv =
          glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
    }

    // Tangent - same coordinate conversion
    if (mesh->HasTangentsAndBitangents()) {
      vertex.tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].z,
                                 -mesh->mTangents[i].y);

      vertex.bitangent =
          glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].z,
                    -mesh->mBitangents[i].y);
    }

    mesh3D->AddVertex(vertex);
  }

  // Process triangles (indices)
  for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
    aiFace &face = mesh->mFaces[i];
    if (face.mNumIndices == 3) {
      mesh3D->AddTriangle(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
    }
  }

  // Process material
  if (mesh->mMaterialIndex >= 0) {
    unsigned int matIndex = mesh->mMaterialIndex;

    // Check cache first
    auto it = materialCache.find(matIndex);
    if (it != materialCache.end()) {
      mesh3D->SetMaterial(it->second);
    } else {
      aiMaterial *material = scene->mMaterials[matIndex];
      auto mat = ProcessMaterial(material, scene, directory, device);
      materialCache[matIndex] = mat;
      mesh3D->SetMaterial(mat);
    }
  }

  // Finalize mesh (create Vulkan buffers)
  if (mesh3D->GetVertexCount() > 0 && mesh3D->GetTriangleCount() > 0) {
    mesh3D->Finalize(device);
  }

  return mesh3D;
}

std::shared_ptr<Material>
ModelImporter::ProcessMaterial(aiMaterial *material, const aiScene *scene,
                               const std::string &directory,
                               Vivid::VividDevice *device) {
  aiString name;
  material->Get(AI_MATKEY_NAME, name);
  auto mat = std::make_shared<Material>(name.C_Str());

  std::cout << "[ModelImporter] Processing material: " << name.C_Str()
            << std::endl;

  // Load diffuse/albedo texture
  if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
    std::cout << "[ModelImporter]   [FOUND] Albedo/Diffuse texture: "
              << texPath.C_Str() << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_SRGB);
    mat->SetAlbedoTexture(tex);
  } else {
    std::cout
        << "[ModelImporter]   [MISSING] Albedo texture (using default white)"
        << std::endl;
    mat->SetAlbedoTexture(GetDefaultTexture(device));
  }

  // Load normal map
  if (material->GetTextureCount(aiTextureType_NORMALS) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_NORMALS, 0, &texPath);
    std::cout << "[ModelImporter]   [FOUND] Normal texture: " << texPath.C_Str()
              << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_UNORM);
    mat->SetNormalTexture(tex);
  } else if (material->GetTextureCount(aiTextureType_HEIGHT) > 0) {
    // Some formats use height map as normal
    aiString texPath;
    material->GetTexture(aiTextureType_HEIGHT, 0, &texPath);
    std::cout << "[ModelImporter]   [FOUND] Height/Normal texture: "
              << texPath.C_Str() << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_UNORM);
    mat->SetNormalTexture(tex);
  } else {
    std::cout << "[ModelImporter]   [MISSING] Normal texture" << std::endl;
  }

  // Load metallic/specular
  if (material->GetTextureCount(aiTextureType_METALNESS) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_METALNESS, 0, &texPath);
    std::cout << "[ModelImporter]   [FOUND] Metalness texture: "
              << texPath.C_Str() << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_UNORM);
    mat->SetMetallicTexture(tex);
  } else if (material->GetTextureCount(aiTextureType_SPECULAR) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_SPECULAR, 0, &texPath);
    std::cout << "[ModelImporter]   [FOUND] Specular texture: "
              << texPath.C_Str() << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_UNORM);
    mat->SetMetallicTexture(tex);
  } else {
    std::cout << "[ModelImporter]   [MISSING] Metallic/Specular texture"
              << std::endl;
  }

  // Load roughness
  if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath);
    std::cout << "[ModelImporter]   [FOUND] Roughness texture: "
              << texPath.C_Str() << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_UNORM);
    mat->SetRoughnessTexture(tex);
  } else if (material->GetTextureCount(aiTextureType_SHININESS) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_SHININESS, 0, &texPath);
    std::cout << "[ModelImporter]   [FOUND] Shininess texture: "
              << texPath.C_Str() << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_UNORM);
    mat->SetRoughnessTexture(tex);
  } else {
    std::cout << "[ModelImporter]   [MISSING] Roughness texture" << std::endl;
  }

  // Load ambient occlusion
  if (material->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texPath);
    std::cout << "[ModelImporter]   AO texture: " << texPath.C_Str()
              << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_UNORM);
    mat->SetAOTexture(tex);
  } else if (material->GetTextureCount(aiTextureType_LIGHTMAP) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_LIGHTMAP, 0, &texPath);
    std::cout << "[ModelImporter]   Lightmap/AO texture: " << texPath.C_Str()
              << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_UNORM);
    mat->SetAOTexture(tex);
  } else {
    std::cout << "[ModelImporter]   No AO texture" << std::endl;
  }

  // Load emissive
  if (material->GetTextureCount(aiTextureType_EMISSIVE) > 0) {
    aiString texPath;
    material->GetTexture(aiTextureType_EMISSIVE, 0, &texPath);
    std::cout << "[ModelImporter]   Emissive texture: " << texPath.C_Str()
              << std::endl;
    auto tex = LoadTexture(texPath.C_Str(), directory, device,
                           VK_FORMAT_R8G8B8A8_SRGB);
    mat->SetEmissiveTexture(tex);
  } else {
    std::cout << "[ModelImporter]   No emissive texture" << std::endl;
  }

  // Ensure all required PBR textures are present (or set to defaults)
  mat->CheckRequiredTextures(device);

  std::cout << "[ModelImporter] Material '" << name.C_Str()
            << "' loaded with pipeline: " << mat->GetPipelineName()
            << std::endl;

  return mat;
}

std::shared_ptr<Vivid::Texture2D>
ModelImporter::LoadTexture(const std::string &texturePath,
                           const std::string &directory,
                           Vivid::VividDevice *device, VkFormat format) {
  namespace fs = std::filesystem;

  // Try multiple path strategies
  std::vector<std::string> pathsToTry;

  // 1. Absolute path as-is
  pathsToTry.push_back(texturePath);

  // 2. Relative to model directory
  pathsToTry.push_back((fs::path(directory) / texturePath).string());

  // 3. Just the filename in model directory
  fs::path texPath(texturePath);
  pathsToTry.push_back((fs::path(directory) / texPath.filename()).string());

  // 4. Check in common texture subdirectories
  pathsToTry.push_back(
      (fs::path(directory) / "textures" / texPath.filename()).string());
  pathsToTry.push_back(
      (fs::path(directory) / "Textures" / texPath.filename()).string());
  pathsToTry.push_back(
      (fs::path(directory) / "tex" / texPath.filename()).string());

  // Try each path
  for (const auto &path : pathsToTry) {
    if (fs::exists(path)) {
      try {
        auto texture = std::make_shared<Vivid::Texture2D>(device, path, format);
        std::cout << "Loaded texture: " << path << std::endl;
        return texture;
      } catch (const std::exception &e) {
        std::cerr << "Failed to load texture " << path << ": " << e.what()
                  << std::endl;
      }
    }
  }

  std::cerr << "Could not find texture: " << texturePath << std::endl;
  return GetDefaultTexture(device);
}

void ModelImporter::SetDefaultTexture(
    std::shared_ptr<Vivid::Texture2D> texture) {
  s_DefaultTexture = texture;
}

std::shared_ptr<Vivid::Texture2D>
ModelImporter::GetDefaultTexture(Vivid::VividDevice *device) {
  if (!s_DefaultTexture && device) {
    // Create a 1x1 white texture
    uint32_t white = 0xFFFFFFFF;
    s_DefaultTexture = std::make_shared<Vivid::Texture2D>(
        device, reinterpret_cast<unsigned char *>(&white), 1, 1, 4);
  }
  return s_DefaultTexture;
}

} // namespace Quantum
