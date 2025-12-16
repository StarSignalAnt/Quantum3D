#pragma once
#include "GraphNode.h"
#include "glm/glm.hpp"

namespace Quantum {

class LightNode : public GraphNode {
public:
  enum class LightType { Point, Directional, Spot };
  LightNode(const std::string &name = "Light",
            LightType type = LightType::Point);
  ~LightNode() override = default;

  LightType GetType() const { return m_Type; }

  void SetColor(const glm::vec3 &color);
  const glm::vec3 &GetColor() const { return m_LightColor; }

  void SetRange(float range) { m_Range = range; }
  float GetRange() const { return m_Range; }

private:
  LightType m_Type;
  glm::vec3 m_LightColor;
  float m_Range = 0.0f; // 0 = infinite range, otherwise max light distance
};

} // namespace Quantum
