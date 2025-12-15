// EngineGlobals.h
#pragma once
#include <memory>

namespace Quantum {
class SceneGraph;
}

class EngineGlobals {
public:
  static std::shared_ptr<Quantum::SceneGraph> EditorScene;
  static void *VulkanDevice; // Adding device for access if needed later, though
                             // Importer takes it as arg
};
