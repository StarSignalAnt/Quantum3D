#include "VividRenderer.h"
#include "pch.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace Vivid {
VividRenderer::VividRenderer(VividDevice *device, int width, int height)
    : m_DevicePtr(device), m_SwapChainPtr(nullptr), m_RenderPassPtr(nullptr),
      m_ImageIndex(0), m_CurrentFrame(0) {
  m_SwapChainPtr = new VividSwapChain(m_DevicePtr, width, height);
  m_RenderPassPtr =
      new VividRenderPass(m_DevicePtr, m_SwapChainPtr->GetImageFormat(),
                          m_SwapChainPtr->GetDepthFormat());

  m_SwapChainPtr->CreateFramebuffers(m_RenderPassPtr->GetRenderPass());

  // Create per-frame command buffers
  m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    m_CommandBuffers[i] =
        new VividCommandBuffer(m_DevicePtr, m_DevicePtr->GetCommandPool());
  }

  CreateSyncObjects();
}

VividRenderer::~VividRenderer() {
  vkDeviceWaitIdle(
      m_DevicePtr->GetDevice()); // Ensure no usage before destruction

  // Destroy per-frame sync objects
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(m_DevicePtr->GetDevice(), m_RenderFinishedSemaphores[i],
                       nullptr);
    vkDestroySemaphore(m_DevicePtr->GetDevice(), m_ImageAvailableSemaphores[i],
                       nullptr);
    vkDestroyFence(m_DevicePtr->GetDevice(), m_InFlightFences[i], nullptr);
  }

  // Destroy per-frame command buffers
  for (auto cmdBuffer : m_CommandBuffers) {
    delete cmdBuffer;
  }
  m_CommandBuffers.clear();

  delete m_RenderPassPtr;
  delete m_SwapChainPtr;
}

void VividRenderer::CreateSyncObjects() {
  m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  // Initialize per-image fence tracking to VK_NULL_HANDLE
  m_ImagesInFlight.resize(m_SwapChainPtr->GetImageCount(), VK_NULL_HANDLE);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(m_DevicePtr->GetDevice(), &semaphoreInfo, nullptr,
                          &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(m_DevicePtr->GetDevice(), &semaphoreInfo, nullptr,
                          &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(m_DevicePtr->GetDevice(), &fenceInfo, nullptr,
                      &m_InFlightFences[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create synchronization objects!");
    }
  }
}

bool VividRenderer::BeginFrame() {
  // Backward compatible: do both phases
  if (!BeginFrameCommandBuffer()) {
    return false;
  }
  BeginMainRenderPass();
  return true;
}

bool VividRenderer::BeginFrameCommandBuffer() {
  // Wait for this frame's fence before reusing its resources
  vkWaitForFences(m_DevicePtr->GetDevice(), 1,
                  &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

  VkResult result = vkAcquireNextImageKHR(
      m_DevicePtr->GetDevice(), m_SwapChainPtr->GetSwapChain(), UINT64_MAX,
      m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE,
      &m_ImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // Check if a previous frame is using this image
  if (m_ImagesInFlight[m_ImageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(m_DevicePtr->GetDevice(), 1,
                    &m_ImagesInFlight[m_ImageIndex], VK_TRUE, UINT64_MAX);
  }
  // Mark this image as now being used by this frame's fence
  m_ImagesInFlight[m_ImageIndex] = m_InFlightFences[m_CurrentFrame];

  // Only reset fence after we know we'll be submitting work
  vkResetFences(m_DevicePtr->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);

  m_CommandBuffers[m_CurrentFrame]->Begin();

  return true;
}

void VividRenderer::BeginMainRenderPass() {
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_RenderPassPtr->GetRenderPass();
  renderPassInfo.framebuffer = m_SwapChainPtr->GetFramebuffers()[m_ImageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = m_SwapChainPtr->GetExtent();

  // Clear both color and depth buffers
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.2f, 0.2f, 0.2f, 1.0f}}; // Black background
  clearValues[1].depthStencil = {1.0f, 0};           // Clear depth to 1.0 (far)
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  m_CommandBuffers[m_CurrentFrame]->BeginRenderPass(renderPassInfo);
}

void VividRenderer::EndFrame() {
  m_CommandBuffers[m_CurrentFrame]->EndRenderPass();
  m_CommandBuffers[m_CurrentFrame]->End();

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  VkCommandBuffer commandBuffers[] = {
      m_CommandBuffers[m_CurrentFrame]->GetCommandBuffer()};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = commandBuffers;

  VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[m_CurrentFrame]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(m_DevicePtr->GetGraphicsQueue(), 1, &submitInfo,
                    m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
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

  // Advance to next frame slot
  m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
} // namespace Vivid
