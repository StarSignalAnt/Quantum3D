#pragma once

#include "VividCommandBuffer.h"
#include "VividDevice.h"
#include "VividRenderPass.h"
#include "VividSwapChain.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace Vivid {

// Maximum number of frames that can be processed concurrently
const int MAX_FRAMES_IN_FLIGHT = 2;

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
    return m_CommandBuffers[m_CurrentFrame]->GetCommandBuffer();
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

  // Per-frame command buffers (one per frame in flight)
  std::vector<VividCommandBuffer *> m_CommandBuffers;

  // Per-frame synchronization objects (indexed by m_CurrentFrame)
  std::vector<VkSemaphore> m_ImageAvailableSemaphores;
  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  std::vector<VkFence> m_InFlightFences;

  // Per-image fence tracking - tracks which fence is associated with each
  // swapchain image
  std::vector<VkFence> m_ImagesInFlight;

  uint32_t m_ImageIndex = 0;
  uint32_t m_CurrentFrame = 0;
};
} // namespace Vivid
