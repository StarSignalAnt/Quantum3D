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

  void SetRange(float range) { m_Range = range; }
  float GetRange() const { return m_Range; }

private:
  glm::vec3 m_LightColor;
  float m_Range = 0.0f; // 0 = infinite range, otherwise max light distance
};

} // namespace Quantum
