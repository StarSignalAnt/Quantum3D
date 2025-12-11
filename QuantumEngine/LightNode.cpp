#include "LightNode.h"

namespace Quantum {

LightNode::LightNode(const std::string &name)
    : GraphNode(name), m_LightColor(1.0f, 1.0f, 1.0f) // Default white
{}

void LightNode::SetColor(const glm::vec3 &color) { m_LightColor = color; }

} // namespace Quantum
