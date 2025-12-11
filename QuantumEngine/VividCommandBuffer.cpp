#include "VividCommandBuffer.h"
#include "pch.h"
#include <stdexcept>


namespace Vivid {
VividCommandBuffer::VividCommandBuffer(VividDevice *device,
                                       VkCommandPool commandPool)
    : m_DevicePtr(device), m_CommandPool(commandPool),
      m_CommandBuffer(VK_NULL_HANDLE) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_CommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  if (vkAllocateCommandBuffers(m_DevicePtr->GetDevice(), &allocInfo,
                               &m_CommandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

VividCommandBuffer::~VividCommandBuffer() {
  if (m_CommandBuffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(m_DevicePtr->GetDevice(), m_CommandPool, 1,
                         &m_CommandBuffer);
  }
}

void VividCommandBuffer::Begin() {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }
}

void VividCommandBuffer::End() {
  if (vkEndCommandBuffer(m_CommandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

void VividCommandBuffer::BeginRenderPass(
    const VkRenderPassBeginInfo &renderPassInfo) {
  vkCmdBeginRenderPass(m_CommandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
}

void VividCommandBuffer::EndRenderPass() {
  vkCmdEndRenderPass(m_CommandBuffer);
}
} // namespace Vivid
