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

  bool BeginFrame();
  void EndFrame();

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
