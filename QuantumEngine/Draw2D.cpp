#include "Draw2D.h"
#include "AppUI.h"

#include "Font.h"
#include "Texture2D.h"

#include "VividBuffer.h"
#include "VividDevice.h"
#include "VividPipeline.h"
#include "VividRenderer.h"
#include "pch.h"
#include <stdexcept>

namespace Vivid {

Draw2D::Draw2D(VividDevice *device, VkRenderPass renderPass)
    : m_DevicePtr(device), m_InstanceBuffer(nullptr) {
  CreateDescriptorSetLayout();
  CreateDescriptorPool();
  CreatePipelines(renderPass);
  CreateInstanceBuffer();
}

Draw2D::~Draw2D() {
  delete m_InstanceBuffer;
  delete m_PipelineSolid;
  delete m_PipelineAlpha;
  delete m_PipelineAdditive;
  vkDestroyDescriptorPool(m_DevicePtr->GetDevice(), m_DescriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(m_DevicePtr->GetDevice(), m_DescriptorSetLayout,
                               nullptr);
}

void Draw2D::CreateDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 0;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = nullptr;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &samplerLayoutBinding;

  if (vkCreateDescriptorSetLayout(m_DevicePtr->GetDevice(), &layoutInfo,
                                  nullptr,
                                  &m_DescriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void Draw2D::CreateDescriptorPool() {
  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 100;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = 100;

  if (vkCreateDescriptorPool(m_DevicePtr->GetDevice(), &poolInfo, nullptr,
                             &m_DescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

void Draw2D::CreatePipelines(VkRenderPass renderPass) {
  const std::string vertPath = "engine/shaders/Basic2D.vert.spv";
  const std::string fragPath = "engine/shaders/Basic2D.frag.spv";

  // Solid - No blending
  BlendConfig solidConfig;
  solidConfig.blendEnable = VK_FALSE;
  m_PipelineSolid =
      new VividPipeline(m_DevicePtr, vertPath, fragPath, m_DescriptorSetLayout,
                        renderPass, solidConfig);

  // Alpha - Standard alpha blending
  BlendConfig alphaConfig;
  alphaConfig.blendEnable = VK_TRUE;
  alphaConfig.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  alphaConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  alphaConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  alphaConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  m_PipelineAlpha =
      new VividPipeline(m_DevicePtr, vertPath, fragPath, m_DescriptorSetLayout,
                        renderPass, alphaConfig);

  // Additive - Additive blending
  BlendConfig additiveConfig;
  additiveConfig.blendEnable = VK_TRUE;
  additiveConfig.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  additiveConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  additiveConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  additiveConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  m_PipelineAdditive =
      new VividPipeline(m_DevicePtr, vertPath, fragPath, m_DescriptorSetLayout,
                        renderPass, additiveConfig);
}

void Draw2D::CreateInstanceBuffer() {
  VkDeviceSize bufferSize = sizeof(SpriteInstance) * MAX_SPRITES;
  m_InstanceBuffer = new VividBuffer(m_DevicePtr, bufferSize,
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_InstanceBuffer->Map();
}

void Draw2D::Begin(VividRenderer *renderer) {
  VkCommandBuffer commandBuffer = renderer->GetCommandBuffer();
  VkExtent2D extent = renderer->GetExtent();
  float width = static_cast<float>(extent.width);
  float height = static_cast<float>(extent.height);

  m_CurrentCommandBuffer = commandBuffer;
  m_ScreenSize = glm::vec2(width, height);
  m_DrawQueue.clear();

  // Initialize scissor stack with full screen
  m_CurrentScissor = glm::vec4(0.0f, 0.0f, width, height);
  m_ScissorStack.clear();
  m_ScissorStack.push_back(m_CurrentScissor);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = width;
  viewport.height = height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = extent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Draw2D::DrawTexture(const glm::vec2 &pos, const glm::vec2 &size,
                         Texture2D *tex, const glm::vec4 &col,
                         BlendMode blend) {
  if (!tex)
    return;

  DrawCommand cmd;
  cmd.texture = tex;
  cmd.blend = blend;
  cmd.instance.pos = pos;
  cmd.instance.size = size;
  cmd.instance.color = col;
  cmd.instance.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); // Full texture UV
  cmd.scissor = m_CurrentScissor;
  m_DrawQueue.push_back(cmd);
}

void Draw2D::DrawRectOutline(const glm::vec2 &pos, const glm::vec2 &size,
                             Texture2D *tex, const glm::vec4 &col,
                             float thickness) {
  if (!tex)
    return;

  // Top
  DrawTexture(pos, glm::vec2(size.x, thickness), tex, col);
  // Bottom
  DrawTexture(glm::vec2(pos.x, pos.y + size.y - thickness),
              glm::vec2(size.x, thickness), tex, col);
  // Left
  DrawTexture(pos, glm::vec2(thickness, size.y), tex, col);
  // Right
  DrawTexture(glm::vec2(pos.x + size.x - thickness, pos.y),
              glm::vec2(thickness, size.y), tex, col);
}

void Draw2D::RenderText(const glm::vec2 &pos, const std::string &text,
                        Font *font, const glm::vec4 &col, BlendMode blend) {
  if (!font || text.empty())
    return;

  Texture2D *atlas = font->GetAtlasTexture();
  if (!atlas)
    return;

  float scale = AppUI::GetScale(); // Get Global Scale
  float scaledLineHeight = font->GetLineHeight() * scale;
  float scaledAscent = font->GetAscent() * scale;

  float cursorX = pos.x;
  float cursorY = pos.y + scaledAscent;

  for (char c : text) {
    if (c == '\n') {
      cursorX = pos.x;
      cursorY += scaledLineHeight;
      continue;
    }

    const GlyphInfo *glyph = font->GetGlyph(c);
    if (!glyph)
      continue;

    // Calculate glyph position & scale
    float glyphX = cursorX + (glyph->xOffset * scale);
    float glyphY = cursorY + (glyph->yOffset * scale);
    float glyphW = glyph->width * scale;
    float glyphH = glyph->height * scale;

    // Queue glyph with proper UV coordinates
    DrawCommand cmd;
    cmd.texture = atlas;
    cmd.blend = blend;
    cmd.instance.pos = glm::vec2(glyphX, glyphY);
    cmd.instance.size = glm::vec2(glyphW, glyphH);
    cmd.instance.color = col;
    cmd.instance.uv = glm::vec4(glyph->u0, glyph->v0, glyph->u1, glyph->v1);
    cmd.scissor = m_CurrentScissor;
    m_DrawQueue.push_back(cmd);

    cursorX += (glyph->xAdvance * scale);
  }
}

void Draw2D::FlushBatch() {
  if (m_DrawQueue.empty())
    return;

  // Upload all instance data
  std::vector<SpriteInstance> instanceData;
  instanceData.reserve(m_DrawQueue.size());
  for (const auto &cmd : m_DrawQueue) {
    instanceData.push_back(cmd.instance);
  }
  m_InstanceBuffer->WriteToBuffer(instanceData.data(),
                                  sizeof(SpriteInstance) * instanceData.size());

  // Bind the instance buffer
  VkBuffer buffers[] = {m_InstanceBuffer->GetBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(m_CurrentCommandBuffer, 0, 1, buffers, offsets);

  // Process batches - consecutive draws with same texture and blend mode
  size_t batchStart = 0;
  while (batchStart < m_DrawQueue.size()) {
    const DrawCommand &first = m_DrawQueue[batchStart];
    size_t batchEnd = batchStart + 1;

    // Find extent of this batch (same texture, blend, AND scissor)
    while (batchEnd < m_DrawQueue.size() &&
           m_DrawQueue[batchEnd].texture == first.texture &&
           m_DrawQueue[batchEnd].blend == first.blend &&
           m_DrawQueue[batchEnd].scissor == first.scissor) {
      batchEnd++;
    }

    // Set scissor for this batch
    VkRect2D scissorRect{};
    scissorRect.offset = {static_cast<int32_t>(first.scissor.x),
                          static_cast<int32_t>(first.scissor.y)};
    scissorRect.extent = {static_cast<uint32_t>(first.scissor.z),
                          static_cast<uint32_t>(first.scissor.w)};
    vkCmdSetScissor(m_CurrentCommandBuffer, 0, 1, &scissorRect);

    uint32_t instanceCount = static_cast<uint32_t>(batchEnd - batchStart);

    // Select pipeline
    VividPipeline *pipeline = nullptr;
    switch (first.blend) {
    case BlendMode::Solid:
      pipeline = m_PipelineSolid;
      break;
    case BlendMode::Alpha:
      pipeline = m_PipelineAlpha;
      break;
    case BlendMode::Additive:
      pipeline = m_PipelineAdditive;
      break;
    }

    pipeline->Bind(m_CurrentCommandBuffer);

    // Push screen size
    vkCmdPushConstants(m_CurrentCommandBuffer, pipeline->GetPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec2),
                       &m_ScreenSize);

    // Bind texture descriptor
    VkDescriptorSet descriptorSet = first.texture->GetDescriptorSet(
        m_DescriptorPool, m_DescriptorSetLayout);
    vkCmdBindDescriptorSets(
        m_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

    // Draw instanced - 6 vertices per quad, instanceCount instances
    // firstInstance = batchStart to offset into instance buffer
    vkCmdDraw(m_CurrentCommandBuffer, 6, instanceCount, 0,
              static_cast<uint32_t>(batchStart));

    batchStart = batchEnd;
  }
}

void Draw2D::End() {
  FlushBatch();
  m_DrawQueue.clear();
  m_CurrentCommandBuffer = VK_NULL_HANDLE;
  m_ScissorStack.clear();
}

// Helper: Intersect two scissor rects
static glm::vec4 IntersectScissor(const glm::vec4 &a, const glm::vec4 &b) {
  float x1 = std::max(a.x, b.x);
  float y1 = std::max(a.y, b.y);
  float x2 = std::min(a.x + a.z, b.x + b.z);
  float y2 = std::min(a.y + a.w, b.y + b.w);
  return glm::vec4(x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1));
}

void Draw2D::PushScissor(const glm::vec4 &rect) {
  // Intersect with current scissor for cascading
  glm::vec4 newScissor = IntersectScissor(m_CurrentScissor, rect);
  m_ScissorStack.push_back(newScissor);
  m_CurrentScissor = newScissor;
}

void Draw2D::PopScissor() {
  if (m_ScissorStack.size() > 1) {
    m_ScissorStack.pop_back();
    m_CurrentScissor = m_ScissorStack.back();
  }
}

glm::vec4 Draw2D::GetCurrentScissor() const { return m_CurrentScissor; }

} // namespace Vivid
