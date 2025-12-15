#pragma once

#include "VividDevice.h"

namespace Vivid {

struct BlendConfig {
  VkBool32 blendEnable = VK_TRUE;
  VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
};

enum class PipelineType {
  Sprite2D, // 2D sprite/UI pipeline with instance data
  Mesh3D    // 3D mesh pipeline with Vertex3D data
};

} // namespace Vivid
