#pragma once

#include "VividDevice.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace Vivid {
class VividSwapChain {
public:
  VividSwapChain(VividDevice *device, int width, int height);
  ~VividSwapChain();

  void CreateFramebuffers(VkRenderPass renderPass);

  VkSwapchainKHR GetSwapChain() { return m_SwapChain; }
  VkFormat GetImageFormat() { return m_SwapChainImageFormat; }
  VkExtent2D GetExtent() { return m_SwapChainExtent; }
  const std::vector<VkImageView> &GetImageViews() {
    return m_SwapChainImageViews;
  }
  const std::vector<VkFramebuffer> &GetFramebuffers() {
    return m_SwapChainFramebuffers;
  }

  VkImageView GetImageView(int index) { return m_SwapChainImageViews[index]; }
  size_t GetImageCount() { return m_SwapChainImages.size(); }

  // Depth buffer access
  VkFormat GetDepthFormat() { return m_DepthFormat; }
  VkImageView GetDepthImageView() { return m_DepthImageView; }

private:
  void CreateSwapChain();
  void CreateImageViews();
  void CreateDepthResources();
  VkFormat FindDepthFormat();

  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  VividDevice *m_DevicePtr;
  int m_Width;
  int m_Height;

  VkSwapchainKHR m_SwapChain;
  std::vector<VkImage> m_SwapChainImages;
  VkFormat m_SwapChainImageFormat;
  VkExtent2D m_SwapChainExtent;
  std::vector<VkImageView> m_SwapChainImageViews;
  std::vector<VkFramebuffer> m_SwapChainFramebuffers;

  // Depth buffer resources
  VkFormat m_DepthFormat = VK_FORMAT_D32_SFLOAT;
  VkImage m_DepthImage = VK_NULL_HANDLE;
  VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
  VkImageView m_DepthImageView = VK_NULL_HANDLE;
};
} // namespace Vivid
