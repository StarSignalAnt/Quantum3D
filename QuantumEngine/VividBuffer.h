#pragma once

#include "VividDevice.h"
#include <vulkan/vulkan.h>

namespace Vivid {
class VividBuffer {
public:
  VividBuffer(VividDevice *device, VkDeviceSize size, VkBufferUsageFlags usage,
              VkMemoryPropertyFlags properties);
  ~VividBuffer();

  VkBuffer GetBuffer() const { return m_Buffer; }
  VkDeviceMemory GetBufferMemory() const { return m_BufferMemory; }

  void Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
  void Unmap();
  void WriteToBuffer(void *data, VkDeviceSize size = VK_WHOLE_SIZE,
                     VkDeviceSize offset = 0);
  void *GetMappedMemory() const { return m_MappedMemory; }

private:
  VividDevice *m_DevicePtr;
  VkBuffer m_Buffer;
  VkDeviceMemory m_BufferMemory;
  VkDeviceSize m_Size;
  void *m_MappedMemory;
};
} // namespace Vivid
