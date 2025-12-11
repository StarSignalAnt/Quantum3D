#pragma once

#include "VividDevice.h"
#include <vulkan/vulkan.h>

namespace Vivid {
class VividCommandBuffer {
public:
  VividCommandBuffer(VividDevice *device, VkCommandPool commandPool);
  ~VividCommandBuffer();

  void Begin();
  void End();

  void BeginRenderPass(const VkRenderPassBeginInfo &renderPassInfo);
  void EndRenderPass();

  VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

private:
  VividDevice *m_DevicePtr;
  VkCommandPool m_CommandPool;
  VkCommandBuffer m_CommandBuffer;
};
} // namespace Vivid
