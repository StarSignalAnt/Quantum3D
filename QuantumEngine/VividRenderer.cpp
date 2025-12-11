#include "VividRenderer.h"
#include "pch.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace Vivid {
VividRenderer::VividRenderer(VividDevice *device, int width, int height)
    : m_DevicePtr(device), m_SwapChainPtr(nullptr), m_RenderPassPtr(nullptr),
      m_CommandBufferPtr(nullptr), m_ImageIndex(0) {
  m_SwapChainPtr = new VividSwapChain(m_DevicePtr, width, height);
  m_RenderPassPtr =
      new VividRenderPass(m_DevicePtr, m_SwapChainPtr->GetImageFormat(),
                          m_SwapChainPtr->GetDepthFormat());

  m_SwapChainPtr->CreateFramebuffers(m_RenderPassPtr->GetRenderPass());

  m_CommandBufferPtr =
      new VividCommandBuffer(m_DevicePtr, m_DevicePtr->GetCommandPool());

  CreateSyncObjects();
}

VividRenderer::~VividRenderer() {
  vkDeviceWaitIdle(
      m_DevicePtr->GetDevice()); // Ensure no usage before destruction

  vkDestroySemaphore(m_DevicePtr->GetDevice(), m_RenderFinishedSemaphore,
                     nullptr);
  vkDestroySemaphore(m_DevicePtr->GetDevice(), m_ImageAvailableSemaphore,
                     nullptr);
  vkDestroyFence(m_DevicePtr->GetDevice(), m_InFlightFence, nullptr);

  delete m_CommandBufferPtr;
  delete m_RenderPassPtr;
  delete m_SwapChainPtr;
}

void VividRenderer::CreateSyncObjects() {
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  if (vkCreateSemaphore(m_DevicePtr->GetDevice(), &semaphoreInfo, nullptr,
                        &m_ImageAvailableSemaphore) != VK_SUCCESS ||
      vkCreateSemaphore(m_DevicePtr->GetDevice(), &semaphoreInfo, nullptr,
                        &m_RenderFinishedSemaphore) != VK_SUCCESS ||
      vkCreateFence(m_DevicePtr->GetDevice(), &fenceInfo, nullptr,
                    &m_InFlightFence) != VK_SUCCESS) {
    throw std::runtime_error("failed to create synchronization objects!");
  }
}

bool VividRenderer::BeginFrame() {
  vkWaitForFences(m_DevicePtr->GetDevice(), 1, &m_InFlightFence, VK_TRUE,
                  UINT64_MAX);

  VkResult result = vkAcquireNextImageKHR(
      m_DevicePtr->GetDevice(), m_SwapChainPtr->GetSwapChain(), UINT64_MAX,
      m_ImageAvailableSemaphore, VK_NULL_HANDLE, &m_ImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  vkResetFences(m_DevicePtr->GetDevice(), 1, &m_InFlightFence);

  m_CommandBufferPtr->Begin();

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_RenderPassPtr->GetRenderPass();
  renderPassInfo.framebuffer = m_SwapChainPtr->GetFramebuffers()[m_ImageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = m_SwapChainPtr->GetExtent();

  // Clear both color and depth buffers
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}}; // Dark gray background
  clearValues[1].depthStencil = {1.0f, 0};           // Clear depth to 1.0 (far)
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  m_CommandBufferPtr->BeginRenderPass(renderPassInfo);

  return true;
}

void VividRenderer::EndFrame() {
  m_CommandBufferPtr->EndRenderPass();

  m_CommandBufferPtr->End();

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphore};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  VkCommandBuffer commandBuffers[] = {m_CommandBufferPtr->GetCommandBuffer()};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = commandBuffers;

  VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphore};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(m_DevicePtr->GetGraphicsQueue(), 1, &submitInfo,
                    m_InFlightFence) != VK_SUCCESS) {
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {m_SwapChainPtr->GetSwapChain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &m_ImageIndex;

  VkResult result =
      vkQueuePresentKHR(m_DevicePtr->GetPresentQueue(), &presentInfo);

  if (result != VK_SUCCESS) {
    if (result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR) {
      std::cerr << "Present failed: " << result << std::endl;
    }
  }
}
} // namespace Vivid
