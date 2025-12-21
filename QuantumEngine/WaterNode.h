#pragma once
#include "GraphNode.h"
#include "Mesh3D.h"
#include "VividDevice.h"

namespace Quantum {

class WaterNode : public GraphNode {
public:
  WaterNode(const std::string &name = "Water");
  virtual ~WaterNode();

  // Initialize rendering resources (Mesh buffers, Material textures)
  void Initialize(Vivid::VividDevice *device);

private:
  void GeneratePlaneMesh();
  void CreateWaterMaterial(Vivid::VividDevice *device);
};

} // namespace Quantum
