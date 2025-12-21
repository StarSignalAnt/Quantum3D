#include "SceneSerializer.h"
#include "../QLang/QClassInstance.h"
#include "../QLang/QRunner.h"
#include "../QuantumEngine/include/nlohmann/json.hpp"
#include "CameraNode.h"
#include "LightNode.h"
#include "ModelImporter.h"
#include "QLangDomain.h"
#include <fstream>
#include <functional>
#include <iostream>


using json = nlohmann::json;

namespace Quantum {

// ============================================================================
// Helper Functions
// ============================================================================

static std::string MakeRelativePath(const std::string &fullPath,
                                    const std::string &contentRoot) {
  // Simple relative path calculation
  if (fullPath.find(contentRoot) == 0) {
    std::string relative = fullPath.substr(contentRoot.length());
    // Remove leading slashes
    while (!relative.empty() && (relative[0] == '/' || relative[0] == '\\')) {
      relative = relative.substr(1);
    }
    return relative;
  }
  return fullPath;
}

static std::string MakeAbsolutePath(const std::string &relativePath,
                                    const std::string &contentRoot) {
  if (relativePath.empty())
    return "";
  // Combine content root with relative path
  std::string result = contentRoot;
  if (!result.empty() && result.back() != '/' && result.back() != '\\') {
    result += '/';
  }
  return result + relativePath;
}

static json Vec3ToJson(const glm::vec3 &v) {
  return json::array({v.x, v.y, v.z});
}

static glm::vec3 JsonToVec3(const json &j) {
  if (j.is_array() && j.size() >= 3) {
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
  }
  return glm::vec3(0.0f);
}

static std::string GetNodeType(GraphNode *node) {
  if (dynamic_cast<CameraNode *>(node))
    return "camera";
  if (dynamic_cast<LightNode *>(node))
    return "light";
  return "node";
}

static std::string LightTypeToString(LightNode::LightType type) {
  switch (type) {
  case LightNode::LightType::Point:
    return "point";
  case LightNode::LightType::Directional:
    return "directional";
  case LightNode::LightType::Spot:
    return "spot";
  default:
    return "point";
  }
}

static LightNode::LightType StringToLightType(const std::string &str) {
  if (str == "directional")
    return LightNode::LightType::Directional;
  if (str == "spot")
    return LightNode::LightType::Spot;
  return LightNode::LightType::Point;
}

// Convert QInstanceValue to JSON
static json InstanceValueToJson(const QInstanceValue &value) {
  return std::visit(
      [](auto &&arg) -> json {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return nullptr;
        } else if constexpr (std::is_same_v<T, bool>) {
          return arg;
        } else if constexpr (std::is_same_v<T, int32_t>) {
          return arg;
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return arg;
        } else if constexpr (std::is_same_v<T, float>) {
          return arg;
        } else if constexpr (std::is_same_v<T, double>) {
          return arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
          return arg;
        } else if constexpr (std::is_same_v<T, void *>) {
          // Can't serialize pointers - return null
          return nullptr;
        } else {
          return nullptr;
        }
      },
      value);
}

// ============================================================================
// Save Implementation
// ============================================================================

static void SerializeNode(json &jNode, GraphNode *node,
                          const std::string &contentRoot,
                          const SceneGraph &scene) {
  // Skip CameraNodes - viewport has its own EditorCamera
  if (dynamic_cast<CameraNode *>(node)) {
    return;
  }

  jNode["name"] = node->GetName();
  jNode["type"] = GetNodeType(node);
  jNode["position"] = Vec3ToJson(node->GetLocalPosition());
  jNode["rotation"] = Vec3ToJson(node->GetRotationEuler());
  jNode["scale"] = Vec3ToJson(node->GetLocalScale());

  // Source path for meshes
  if (!node->GetSourcePath().empty()) {
    jNode["meshSource"] = MakeRelativePath(node->GetSourcePath(), contentRoot);
  }

  // Light-specific properties
  if (auto light = dynamic_cast<LightNode *>(node)) {
    jNode["lightType"] = LightTypeToString(light->GetType());
    jNode["color"] = Vec3ToJson(light->GetColor());
    jNode["range"] = light->GetRange();
  }

  // Scripts
  json jScripts = json::array();
  for (const auto &script : node->GetScripts()) {
    json jScript;
    jScript["class"] = script->GetQClassName();

    // Serialize all members
    json jMembers;
    for (const auto &[name, value] : script->GetMembers()) {
      json jVal = InstanceValueToJson(value);

      // If result is null, it might be a node pointer (void*)
      if (jVal.is_null() && std::holds_alternative<void *>(value)) {
        void *ptr = std::get<void *>(value);
        if (ptr) {
          // Check if ptr is a node in the scene
          GraphNode *refNode = scene.FindNodeByPointer(ptr);
          if (refNode) {
            // Save as @node:NodeName
            jVal = "@node:" + refNode->GetFullName();
          }
        }
      }

      jMembers[name] = jVal;
    }

    // Serialize nested instances (GameNode references, etc.)
    for (const auto &nestedName : script->GetNestedInstanceNames()) {
      auto nested = script->GetNestedInstance(nestedName);
      if (nested) {
        // Check if it's a GameNode reference by checking for "NodePtr" member
        auto nodeMember = nested->GetMember("NodePtr");
        if (std::holds_alternative<void *>(nodeMember)) {
          // This is a node reference - save as @node:NodeName
          void *nodePtr = std::get<void *>(nodeMember);
          if (nodePtr) {
            GraphNode *refNode = static_cast<GraphNode *>(nodePtr);
            jMembers[nestedName] = "@node:" + refNode->GetFullName();
          }
        }
      }
    }

    jScript["members"] = jMembers;
    jScripts.push_back(jScript);
  }
  if (!jScripts.empty()) {
    jNode["scripts"] = jScripts;
  }

  // Children
  json jChildren = json::array();
  for (const auto &child : node->GetChildren()) {
    json jChild;
    SerializeNode(jChild, child.get(), contentRoot, scene);
    // Only add non-empty nodes (cameras are skipped and produce empty json)
    if (!jChild.empty()) {
      jChildren.push_back(jChild);
    }
  }
  if (!jChildren.empty()) {
    jNode["children"] = jChildren;
  }
}

bool SceneSerializer::Save(const SceneGraph &scene, const std::string &filepath,
                           const std::string &contentRoot, float editorYaw,
                           float editorPitch) {
  try {
    json root;
    root["version"] = 1;

    // Save editor camera state (yaw/pitch from EditorCamera, position from
    // CameraNode)
    auto camera = scene.GetCurrentCamera();
    if (camera) {
      json jCamera;
      jCamera["position"] = Vec3ToJson(camera->GetLocalPosition());
      jCamera["yaw"] = editorYaw;
      jCamera["pitch"] = editorPitch;
      root["editorCamera"] = jCamera;
    }

    // Serialize scene nodes (skip the root node itself, just serialize its
    // children)
    json jNodes = json::array();
    GraphNode *sceneRoot = scene.GetRoot();
    if (sceneRoot) {
      for (const auto &child : sceneRoot->GetChildren()) {
        json jNode;
        SerializeNode(jNode, child.get(), contentRoot, scene);
        // Only add non-empty nodes (cameras are skipped and produce empty json)
        if (!jNode.empty()) {
          jNodes.push_back(jNode);
        }
      }
    }
    root["nodes"] = jNodes;

    // Write to file
    std::ofstream file(filepath);
    if (!file.is_open()) {
      std::cerr << "[SceneSerializer] Failed to open file for writing: "
                << filepath << std::endl;
      return false;
    }

    file << root.dump(2); // Pretty print with 2-space indentation
    file.close();

    std::cout << "[SceneSerializer] Scene saved to: " << filepath << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[SceneSerializer] Save failed: " << e.what() << std::endl;
    return false;
  }
}

// ============================================================================
// Load Implementation
// ============================================================================

static std::shared_ptr<GraphNode>
DeserializeNode(const json &jNode, GraphNode *parent,
                const std::string &contentRoot, Vivid::VividDevice *device,
                QLangDomain *domain, SceneGraph &scene,
                std::vector<SceneSerializer::DeferredNodeRef> &deferredRefs) {
  std::string name = jNode.value("name", "Node");
  std::string type = jNode.value("type", "node");

  std::shared_ptr<GraphNode> node;

  // Create appropriate node type
  if (type == "camera") {
    // Skip CameraNodes - viewport has its own EditorCamera
    return nullptr;
  } else if (type == "light") {
    std::string lightTypeStr = jNode.value("lightType", "point");
    auto light =
        std::make_shared<LightNode>(name, StringToLightType(lightTypeStr));

    if (jNode.contains("color")) {
      light->SetColor(JsonToVec3(jNode["color"]));
    }
    if (jNode.contains("range")) {
      light->SetRange(jNode["range"].get<float>());
    }

    scene.AddLight(light);
    node = light;

    // Set transform for light
    if (jNode.contains("position")) {
      node->SetLocalPosition(JsonToVec3(jNode["position"]));
    }
    if (jNode.contains("rotation")) {
      node->SetRotationEuler(JsonToVec3(jNode["rotation"]));
    }
    if (jNode.contains("scale")) {
      node->SetLocalScale(JsonToVec3(jNode["scale"]));
    }
  } else if (jNode.contains("meshSource")) {
    // For nodes with mesh source, use the imported model directly
    // This preserves the internal hierarchy and transforms from ModelImporter
    std::string meshPath =
        MakeAbsolutePath(jNode["meshSource"].get<std::string>(), contentRoot);

    auto imported = ModelImporter::ImportEntity(meshPath, device);
    if (imported) {
      // Use imported model directly - it has proper hierarchy/transforms
      node = imported;
      node->SetName(name);
      node->SetSourcePath(meshPath);

      // Apply saved transform ON TOP of imported transform
      // Note: For imported models, we typically save and restore position only
      // since the model's internal transforms handle the rest
      if (jNode.contains("position")) {
        node->SetLocalPosition(JsonToVec3(jNode["position"]));
      }
      if (jNode.contains("scale")) {
        node->SetLocalScale(JsonToVec3(jNode["scale"]));
      }
      // Rotation is tricky - imported models may have coordinate conversion
      // Only apply if explicitly saved and non-zero
      if (jNode.contains("rotation")) {
        glm::vec3 rot = JsonToVec3(jNode["rotation"]);
        if (rot.x != 0.0f || rot.y != 0.0f || rot.z != 0.0f) {
          node->SetRotationEuler(rot);
        }
      }
    } else {
      // Fallback if import fails - create empty node
      node = std::make_shared<GraphNode>(name);
    }
  } else {
    // Regular node without mesh
    node = std::make_shared<GraphNode>(name);

    if (jNode.contains("position")) {
      node->SetLocalPosition(JsonToVec3(jNode["position"]));
    }
    if (jNode.contains("rotation")) {
      node->SetRotationEuler(JsonToVec3(jNode["rotation"]));
    }
    if (jNode.contains("scale")) {
      node->SetLocalScale(JsonToVec3(jNode["scale"]));
    }
  }

  // Add to parent
  if (parent) {
    parent->AddChild(node);
  }

  // Load scripts (deferred node references handled later)
  if (jNode.contains("scripts") && domain) {
    for (const auto &jScript : jNode["scripts"]) {
      std::string className = jScript.value("class", "");
      if (className.empty())
        continue;

      // Create a new instance - need to find the script path
      // For now, we'll use LoadClass with just the class name
      // This requires the script to already be registered
      auto runner = domain->GetRunner();
      if (runner) {
        auto classInst = runner->CreateInstance(className);
        if (classInst) {
          // Set member values
          if (jScript.contains("members")) {
            for (auto &[memberName, memberValue] : jScript["members"].items()) {
              if (memberValue.is_string()) {
                std::string strVal = memberValue.get<std::string>();
                // Check for node reference
                if (strVal.rfind("@node:", 0) == 0) {
                  // Deferred node reference
                  std::string targetName = strVal.substr(6);
                  deferredRefs.push_back({classInst, memberName, targetName});
                } else {
                  classInst->SetMember(memberName, strVal);
                }
              } else if (memberValue.is_boolean()) {
                classInst->SetMember(memberName, memberValue.get<bool>());
              } else if (memberValue.is_number_integer()) {
                classInst->SetMember(memberName, memberValue.get<int32_t>());
              } else if (memberValue.is_number_float()) {
                classInst->SetMember(memberName, memberValue.get<float>());
              }
            }
          }

          // Set the Node reference in the script
          classInst->SetMember("Node", static_cast<void *>(node.get()));

          node->AddScript(classInst);
        }
      }
    }
  }

  // Recursively load children
  if (jNode.contains("children")) {
    // Check if this is an imported model (has meshSource)
    bool hasMeshSource = jNode.contains("meshSource");

    for (const auto &jChild : jNode["children"]) {
      std::string childName = jChild.value("name", "");

      if (hasMeshSource && !childName.empty()) {
        // For imported models, match children by name instead of creating new
        GraphNode *matchedChild = nullptr;
        for (const auto &existingChild : node->GetChildren()) {
          if (existingChild->GetName() == childName) {
            matchedChild = existingChild.get();
            break;
          }
        }

        if (matchedChild) {
          // Apply saved transform to existing imported child
          if (jChild.contains("position")) {
            matchedChild->SetLocalPosition(JsonToVec3(jChild["position"]));
          }
          if (jChild.contains("rotation")) {
            glm::vec3 rot = JsonToVec3(jChild["rotation"]);
            if (rot.x != 0.0f || rot.y != 0.0f || rot.z != 0.0f) {
              matchedChild->SetRotationEuler(rot);
            }
          }
          if (jChild.contains("scale")) {
            matchedChild->SetLocalScale(JsonToVec3(jChild["scale"]));
          }

          // Also load scripts for matched child nodes
          if (jChild.contains("scripts") && domain) {
            for (const auto &jScript : jChild["scripts"]) {
              std::string className = jScript.value("class", "");
              if (className.empty())
                continue;

              auto runner = domain->GetRunner();
              if (runner) {
                auto classInst = runner->CreateInstance(className);
                if (classInst) {
                  // Set member values
                  if (jScript.contains("members")) {
                    for (auto &[memberName, memberValue] :
                         jScript["members"].items()) {
                      if (memberValue.is_string()) {
                        std::string strVal = memberValue.get<std::string>();
                        if (strVal.rfind("@node:", 0) == 0) {
                          std::string targetName = strVal.substr(6);
                          deferredRefs.push_back(
                              {classInst, memberName, targetName});
                        } else {
                          classInst->SetMember(memberName, strVal);
                        }
                      } else if (memberValue.is_boolean()) {
                        classInst->SetMember(memberName,
                                             memberValue.get<bool>());
                      } else if (memberValue.is_number_integer()) {
                        classInst->SetMember(memberName,
                                             memberValue.get<int32_t>());
                      } else if (memberValue.is_number_float()) {
                        classInst->SetMember(memberName,
                                             memberValue.get<float>());
                      }
                    }
                  }

                  // Set the Node reference in the script
                  classInst->SetMember("NodePtr",
                                       static_cast<void *>(matchedChild));

                  matchedChild->AddScript(classInst);
                }
              }
            }
          }

          continue; // Skip normal deserialization
        }
      }

      // Normal deserialization for non-imported or unmatched children
      DeserializeNode(jChild, node.get(), contentRoot, device, domain, scene,
                      deferredRefs);
    }
  }

  return node;
}

bool SceneSerializer::Load(SceneGraph &scene, const std::string &filepath,
                           const std::string &contentRoot,
                           Vivid::VividDevice *device, QLangDomain *domain,
                           LoadedCameraState *outCameraState) {
  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      std::cerr << "[SceneSerializer] Failed to open file for reading: "
                << filepath << std::endl;
      return false;
    }

    json root;
    file >> root;
    file.close();

    // Check version
    int version = root.value("version", 0);
    if (version != 1) {
      std::cerr << "[SceneSerializer] Unsupported file version: " << version
                << std::endl;
      return false;
    }

    // Clear existing scene
    scene.Clear();

    // Track deferred references
    std::vector<DeferredNodeRef> deferredRefs;

    // Load nodes
    if (root.contains("nodes")) {
      for (const auto &jNode : root["nodes"]) {
        DeserializeNode(jNode, scene.GetRoot(), contentRoot, device, domain,
                        scene, deferredRefs);
      }
    }

    // Resolve deferred node references
    for (const auto &ref : deferredRefs) {
      if (!ref.scriptInstance)
        continue;

      auto targetNode = scene.FindNode(ref.targetNodeName);
      if (targetNode) {
        bool handled = false;

        // 1. Check if the member corresponds to an EXISTING nested instance
        if (ref.scriptInstance->HasNestedInstance(ref.memberName)) {
          auto nested = ref.scriptInstance->GetNestedInstance(ref.memberName);
          if (nested) {
            nested->SetMember("NodePtr", static_cast<void *>(targetNode.get()));
            handled = true;
          }
        }

        // 2. If not existing, check if it SHOULD be a nested instance (GameNode
        // subclass)
        if (!handled) {
          auto classDef = ref.scriptInstance->GetClassDef();
          if (classDef && domain) {
            // Find the member definition
            std::shared_ptr<QVariableDecl> member = nullptr;
            for (const auto &m : classDef->GetMembers()) {
              if (m->GetName() == ref.memberName) {
                member = m;
                break;
              }
            }

            if (member) {
              std::string typeName = member->GetTypeName();

              // Helper check for GameNode inheritance
              auto isGameNodeClass = [&](const std::string &clsName) {
                if (clsName == "GameNode")
                  return true;
                auto runner = domain->GetRunner();
                if (!runner)
                  return false;
                auto cls = runner->FindClass(clsName);
                while (cls) {
                  if (cls->GetName() == "GameNode")
                    return true;
                  if (!cls->HasParent())
                    break;
                  cls = runner->FindClass(cls->GetParentClassName());
                }
                return false;
              };

              if (isGameNodeClass(typeName)) {
                // It IS a GameNode type - create the nested instance wrapper
                auto runner = domain->GetRunner();
                if (runner) {
                  auto newInst = runner->CreateInstance(typeName);
                  if (newInst) {
                    // Set the pointer
                    newInst->SetMember("NodePtr",
                                       static_cast<void *>(targetNode.get()));
                    // Set as nested instance
                    ref.scriptInstance->SetNestedInstance(ref.memberName,
                                                          newInst);
                    handled = true;
                  }
                }
              }
            }
          }
        }

        // 3. Fallback: It's a regular member (void*)
        if (!handled) {
          ref.scriptInstance->SetMember(ref.memberName,
                                        static_cast<void *>(targetNode.get()));
        }
      } else {
        std::cerr << "[SceneSerializer] Failed to resolve node reference: "
                  << ref.targetNodeName << std::endl;
      }
    }

    // Restore editor camera transform and output yaw/pitch
    if (root.contains("editorCamera")) {
      auto camera = scene.GetCurrentCamera();
      const auto &jCamera = root["editorCamera"];
      if (camera) {
        if (jCamera.contains("position")) {
          camera->SetLocalPosition(JsonToVec3(jCamera["position"]));
        }
      }
      // Return yaw/pitch to caller
      if (outCameraState) {
        outCameraState->yaw = jCamera.value("yaw", 0.0f);
        outCameraState->pitch = jCamera.value("pitch", 0.0f);
        outCameraState->hasData = true;
        std::cout << "[SceneSerializer] Loaded editor camera: yaw="
                  << outCameraState->yaw << " pitch=" << outCameraState->pitch
                  << std::endl;
      }
    }

    std::cout << "[SceneSerializer] Scene loaded from: " << filepath
              << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[SceneSerializer] Load failed: " << e.what() << std::endl;
    return false;
  }
}

} // namespace Quantum
