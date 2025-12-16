#include "LightNode.h"

namespace Quantum {

LightNode::LightNode(const std::string &name, LightType type)
    : GraphNode(name), m_Type(type),
      m_LightColor(1.0f, 1.0f, 1.0f) // Default white
{}

void LightNode::SetColor(const glm::vec3 &color) { m_LightColor = color; }

} // namespace Quantum
