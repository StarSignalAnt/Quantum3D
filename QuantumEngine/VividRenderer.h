#pragma once

#include "VividCommandBuffer.h"
#include "VividDevice.h"
#include "VividRenderPass.h"
#include "VividSwapChain.h"
#include <vulkan/vulkan.h>

namespace Vivid {
class VividRenderer {
public:
  VividRenderer(VividDevice *device, int width, int height);
  ~VividRenderer();

  // Standard rendering - begins command buffer AND render pass
  bool BeginFrame();
  void EndFrame();

  // Split-phase rendering for shadow pass injection:
  // 1. BeginFrameCommandBuffer() - starts command buffer, acquires image
  // 2. (Caller does shadow render passes here)
  // 3. BeginMainRenderPass() - starts the main render pass
  // 4. (Caller does main rendering)
  // 5. EndFrame() - ends render pass and submits
  bool BeginFrameCommandBuffer();
  void BeginMainRenderPass();

  VkCommandBuffer GetCommandBuffer() const {
    return m_CommandBufferPtr->GetCommandBuffer();
  }

  VkRenderPass GetRenderPass() const {
    return m_RenderPassPtr->GetRenderPass();
  }

  VkExtent2D GetExtent() const { return m_SwapChainPtr->GetExtent(); }

private:
  void CreateSyncObjects();

  VividDevice *m_DevicePtr;
  VividSwapChain *m_SwapChainPtr;
  VividRenderPass *m_RenderPassPtr;
  VividCommandBuffer *m_CommandBufferPtr;

  VkSemaphore m_ImageAvailableSemaphore;
  VkSemaphore m_RenderFinishedSemaphore;
  VkFence m_InFlightFence;
  uint32_t m_ImageIndex = 0;
};
} // namespace Vivid
