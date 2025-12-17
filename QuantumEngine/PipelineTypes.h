#pragma once

#include "VividDevice.h"

namespace Vivid {

struct BlendConfig {
  // Color blending
  VkBool32 blendEnable = VK_TRUE;
  VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

  // Depth configuration (for multi-light additive passes)
  VkBool32 depthTestEnable = VK_TRUE; // Set to VK_FALSE to disable depth test
  VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
  VkBool32 depthWriteEnable = VK_TRUE;

  // Depth bias (for avoiding z-fighting in multi-pass rendering)
  VkBool32 depthBiasEnable = VK_FALSE;
  float depthBiasConstantFactor = 0.0f;
  float depthBiasSlopeFactor = 0.0f;

  // Rasterization State
  VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
  float lineWidth = 1.0f;
  VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;

  // Push constant size (0 = use default 8 bytes for screen_size)
  uint32_t pushConstantSize = 0;
};

enum class PipelineType {
  Sprite2D, // 2D sprite/UI pipeline with instance data
  Mesh3D    // 3D mesh pipeline with Vertex3D data
};

} // namespace Vivid
