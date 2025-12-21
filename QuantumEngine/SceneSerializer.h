#pragma once

#include "../QLang/QRunner.h"
#include "SceneGraph.h"
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class QLangDomain;
class QClassInstance;

namespace Vivid {
class VividDevice;
}

namespace Quantum {

/// <summary>
/// Handles serialization and deserialization of scene graphs to/from .graph
/// files. Uses JSON format with asset references (meshes, textures, scripts).
/// </summary>
class SceneSerializer {
public:
  // Deferred reference for GameNode members in scripts
  struct DeferredNodeRef {
    std::shared_ptr<QClassInstance> scriptInstance;
    std::string memberName;
    std::string targetNodeName;
  };

  /// Saved editor camera state returned from Load
  struct LoadedCameraState {
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool hasData = false;
  };

  /// <summary>
  /// Save a scene graph to a .graph file.
  /// </summary>
  /// <param name="scene">The scene graph to save</param>
  /// <param name="filepath">Full path to the output file</param>
  /// <param name="contentRoot">Content root for relative path
  /// calculation</param>
  /// <param name="editorYaw">Editor camera yaw</param>
  /// <param name="editorPitch">Editor camera pitch</param>
  /// <returns>True if save was successful</returns>
  static bool Save(const SceneGraph &scene, const std::string &filepath,
                   const std::string &contentRoot, float editorYaw = 0.0f,
                   float editorPitch = 0.0f);

  /// <summary>
  /// Load a scene graph from a .graph file.
  /// </summary>
  /// <param name="scene">The scene graph to populate (will be cleared
  /// first)</param>
  /// <param name="filepath">Full path to the input file</param>
  /// <param name="contentRoot">Content root for resolving relative
  /// paths</param>
  /// <param name="device">Vulkan device for mesh/texture loading</param>
  /// <param name="domain">QLang domain for script loading</param>
  /// <returns>True if load was successful</returns>
  static bool Load(SceneGraph &scene, const std::string &filepath,
                   const std::string &contentRoot, Vivid::VividDevice *device,
                   QLangDomain *domain,
                   LoadedCameraState *outCameraState = nullptr);
};

} // namespace Quantum
