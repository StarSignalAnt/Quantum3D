#include "VividBuffer.h"
#include "pch.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace Vivid {
VividBuffer::VividBuffer(VividDevice *device, VkDeviceSize size,
                         VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties)
    : m_DevicePtr(device), m_Size(size), m_MappedMemory(nullptr) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_DevicePtr->GetDevice(), &bufferInfo, nullptr,
                     &m_Buffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_DevicePtr->GetDevice(), m_Buffer,
                                &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      m_DevicePtr->FindMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(m_DevicePtr->GetDevice(), &allocInfo, nullptr,
                       &m_BufferMemory) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate buffer memory!");
  }

  vkBindBufferMemory(m_DevicePtr->GetDevice(), m_Buffer, m_BufferMemory, 0);
}

VividBuffer::~VividBuffer() {
  if (m_MappedMemory) {
    Unmap();
  }
  vkDestroyBuffer(m_DevicePtr->GetDevice(), m_Buffer, nullptr);
  vkFreeMemory(m_DevicePtr->GetDevice(), m_BufferMemory, nullptr);
}

void VividBuffer::Map(VkDeviceSize size, VkDeviceSize offset) {
  if (vkMapMemory(m_DevicePtr->GetDevice(), m_BufferMemory, offset, size, 0,
                  &m_MappedMemory) != VK_SUCCESS) {
    throw std::runtime_error("failed to map buffer memory!");
  }
}

void VividBuffer::Unmap() {
  vkUnmapMemory(m_DevicePtr->GetDevice(), m_BufferMemory);
  m_MappedMemory = nullptr;
}

void VividBuffer::WriteToBuffer(void *data, VkDeviceSize size,
                                VkDeviceSize offset) {
  if (size == VK_WHOLE_SIZE) {
    size = m_Size;
  }

  if (!m_MappedMemory) {
    Map(size, offset);
  }
  memcpy(m_MappedMemory, data, (size_t)size);
  // We could unmap here, but keeping it mapped might be useful.
  // For staging buffers used once, we should probably unmap.
  // For simplicity, let's keep it manual or unmap in destructor.
}
} // namespace Vivid
