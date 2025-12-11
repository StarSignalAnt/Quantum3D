#pragma once

#include "Font.h"
#include "Texture2D.h"
#include "VividBuffer.h"
#include "VividDevice.h"
#include "VividPipeline.h"
#include "VividRenderer.h"
#include "glm/glm.hpp"
#include <vector>
#include <vulkan/vulkan.h>

namespace Vivid {

enum class BlendMode {
  Solid,   // No blending, fully opaque
  Alpha,   // Standard alpha blending (src.a, 1 - src.a)
  Additive // Additive blending (src.a, 1)
};

// Per-instance data for batching (includes UV for text)
struct SpriteInstance {
  glm::vec2 pos;
  glm::vec2 size;
  glm::vec4 color;
  glm::vec4 uv; // u0, v0, u1, v1 (default: 0,0,1,1 for full texture)
};

// Queued draw command
struct DrawCommand {
  Texture2D *texture;
  BlendMode blend;
  SpriteInstance instance;
  glm::vec4 scissor; // x, y, width, height in pixels
};

class Draw2D {
public:
  Draw2D(VividDevice *device, VkRenderPass renderPass);
  ~Draw2D();

  void Begin(VividRenderer *renderer);

  void DrawTexture(const glm::vec2 &pos, const glm::vec2 &size, Texture2D *tex,
                   const glm::vec4 &col = glm::vec4(1.0f),
                   BlendMode blend = BlendMode::Alpha);

  void DrawRectOutline(const glm::vec2 &pos, const glm::vec2 &size,
                       Texture2D *tex, const glm::vec4 &col,
                       float thickness = 1.0f);

  void DrawText(const glm::vec2 &pos, const std::string &text, Font *font,
                const glm::vec4 &col = glm::vec4(1.0f),
                BlendMode blend = BlendMode::Alpha);

  void End();

  // Scissor clipping
  void PushScissor(const glm::vec4 &rect); // Intersects with current, pushes
  void PopScissor();                       // Pops and restores previous
  glm::vec4 GetCurrentScissor() const;     // Returns current scissor rect

private:
  void CreateDescriptorSetLayout();
  void CreateDescriptorPool();
  void CreatePipelines(VkRenderPass renderPass);
  void CreateInstanceBuffer();
  void FlushBatch();

  VividDevice *m_DevicePtr;
  VividPipeline *m_PipelineSolid;
  VividPipeline *m_PipelineAlpha;
  VividPipeline *m_PipelineAdditive;

  VkDescriptorSetLayout m_DescriptorSetLayout;
  VkDescriptorPool m_DescriptorPool;
  VkCommandBuffer m_CurrentCommandBuffer;
  glm::vec2 m_ScreenSize;

  // Scissor stack for cascaded clipping
  std::vector<glm::vec4> m_ScissorStack;
  glm::vec4 m_CurrentScissor;

  // Batching
  std::vector<DrawCommand> m_DrawQueue;
  VividBuffer *m_InstanceBuffer;
  static constexpr size_t MAX_SPRITES = 10000;
};
} // namespace Vivid
