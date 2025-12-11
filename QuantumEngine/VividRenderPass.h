#pragma once

#include "VividDevice.h"
#include <vulkan/vulkan.h>

namespace Vivid {
class VividRenderPass {
public:
  VividRenderPass(VividDevice *device, VkFormat imageFormat,
                  VkFormat depthFormat);
  ~VividRenderPass();

  VkRenderPass GetRenderPass() const { return m_RenderPass; }

private:
  VividDevice *m_DevicePtr;
  VkRenderPass m_RenderPass;
};
} // namespace Vivid
