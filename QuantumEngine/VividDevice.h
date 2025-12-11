#pragma once

#include "pch.h"
#include <optional>
#include <vector>

// Windows headers must be included BEFORE Vulkan Win32 headers
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX // Prevent Windows min/max macros from conflicting with std::
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#endif

struct GLFWwindow;

namespace Vivid {
struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

class VividDevice {
public:
  // Original GLFW-based constructor
  VividDevice(GLFWwindow *window, const char *title);

  // New Win32-based constructor for Qt integration
#ifdef _WIN32
  VividDevice(HWND hwnd, HINSTANCE hinstance, const char *title);
#endif

  ~VividDevice();

  VkDevice GetDevice() { return m_Device; }
  VkPhysicalDevice GetPhysicalDevice() { return m_PhysicalDevice; }
  VkSurfaceKHR GetSurface() { return m_Surface; }
  VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
  VkQueue GetPresentQueue() { return m_PresentQueue; }
  VkCommandPool GetCommandPool() { return m_CommandPool; }

  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

  uint32_t FindMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);
  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer &buffer,
                    VkDeviceMemory &bufferMemory);
  void CreateImage(uint32_t width, uint32_t height, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkImage &image,
                   VkDeviceMemory &imageMemory);
  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout);
  void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                         uint32_t height);
  VkImageView CreateImageView(VkImage image, VkFormat format);

  VkCommandBuffer BeginSingleTimeCommands();
  void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
  void CreateInstance(const char *title, bool useGLFW);
  void SetupDebugMessenger();
  void CreateSurface();
#ifdef _WIN32
  void CreateWin32Surface();
#endif
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateCommandPool();
  void InitCommon(const char *title);

  bool IsDeviceSuitable(VkPhysicalDevice device);
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

  bool CheckValidationLayerSupport();

  // GLFW window (for GLFW path)
  GLFWwindow *m_Window = nullptr;

  // Win32 handles (for Qt path)
#ifdef _WIN32
  HWND m_Hwnd = nullptr;
  HINSTANCE m_Hinstance = nullptr;
#endif

  bool m_UseGLFW = true;

  VkInstance m_Instance;
  VkDebugUtilsMessengerEXT m_DebugMessenger;
  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
  VkDevice m_Device;
  VkSurfaceKHR m_Surface;
  VkQueue m_GraphicsQueue;
  VkQueue m_PresentQueue;
  VkCommandPool m_CommandPool;
};
} // namespace Vivid
