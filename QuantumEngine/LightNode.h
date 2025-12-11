#pragma once
#include "GraphNode.h"
#include "glm/glm.hpp"

namespace Quantum {

class LightNode : public GraphNode {
public:
  LightNode(const std::string &name = "Light");
  ~LightNode() override = default;

  void SetColor(const glm::vec3 &color);
  const glm::vec3 &GetColor() const { return m_LightColor; }

private:
  glm::vec3 m_LightColor;
};

} // namespace Quantum
