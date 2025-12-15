#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "PointShadowMap.h"
#include <iostream>
#include <stdexcept>

namespace Quantum {

PointShadowMap::~PointShadowMap() { Shutdown(); }

void PointShadowMap::Initialize(Vivid::VividDevice *device,
                                uint32_t resolution) {
  if (m_Initialized) {
    return;
  }

  m_Device = device;
  m_Resolution = resolution;

  std::cout << "[PointShadowMap] Initializing cube shadow map (" << resolution
            << "x" << resolution << "x6)" << std::endl;

  CreateCubeImage();
  TransitionToShaderReadable(); // Transition to valid layout for shader
                                // sampling
  CreateCubeImageView();
  CreateFaceImageViews();
  CreateSampler();
  CreateRenderPass();
  CreateFramebuffers();

  m_Initialized = true;
  std::cout << "[PointShadowMap] Initialization complete" << std::endl;
}

void PointShadowMap::Shutdown() {
  if (!m_Device || !m_Initialized) {
    return;
  }

  VkDevice device = m_Device->GetDevice();

  // Destroy framebuffers
  for (auto &fb : m_Framebuffers) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
      fb = VK_NULL_HANDLE;
    }
  }

  // Destroy render pass
  if (m_RenderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_RenderPass, nullptr);
    m_RenderPass = VK_NULL_HANDLE;
  }

  // Destroy sampler
  if (m_Sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_Sampler, nullptr);
    m_Sampler = VK_NULL_HANDLE;
  }

  // Destroy face image views
  for (auto &view : m_FaceImageViews) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, view, nullptr);
      view = VK_NULL_HANDLE;
    }
  }

  // Destroy cube image view
  if (m_CubeImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(device, m_CubeImageView, nullptr);
    m_CubeImageView = VK_NULL_HANDLE;
  }

  // Destroy image
  if (m_CubeImage != VK_NULL_HANDLE) {
    vkDestroyImage(device, m_CubeImage, nullptr);
    m_CubeImage = VK_NULL_HANDLE;
  }

  // Free memory
  if (m_CubeMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, m_CubeMemory, nullptr);
    m_CubeMemory = VK_NULL_HANDLE;
  }

  m_Initialized = false;
}

void PointShadowMap::CreateCubeImage() {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = m_Resolution;
  imageInfo.extent.height = m_Resolution;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = NUM_FACES; // 6 faces for cube map
  imageInfo.format = VK_FORMAT_D32_SFLOAT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  if (vkCreateImage(m_Device->GetDevice(), &imageInfo, nullptr, &m_CubeImage) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create cube shadow map image!");
  }

  // Allocate memory
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(m_Device->GetDevice(), m_CubeImage,
                               &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = m_Device->FindMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(m_Device->GetDevice(), &allocInfo, nullptr,
                       &m_CubeMemory) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate cube shadow map memory!");
  }

  vkBindImageMemory(m_Device->GetDevice(), m_CubeImage, m_CubeMemory, 0);
}

void PointShadowMap::TransitionToShaderReadable() {
  // Create a one-time command buffer to transition the image layout
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_Device->GetCommandPool();
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(m_Device->GetDevice(), &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  // Transition all 6 layers from UNDEFINED to SHADER_READ_ONLY_OPTIMAL
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_CubeImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = NUM_FACES; // All 6 faces
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkEndCommandBuffer(commandBuffer);

  // Submit and wait
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_Device->GetGraphicsQueue());

  vkFreeCommandBuffers(m_Device->GetDevice(), m_Device->GetCommandPool(), 1,
                       &commandBuffer);

  std::cout << "[PointShadowMap] Image layout transitioned to SHADER_READ_ONLY"
            << std::endl;
}

void PointShadowMap::CreateCubeImageView() {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_CubeImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  viewInfo.format = VK_FORMAT_D32_SFLOAT;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = NUM_FACES;

  if (vkCreateImageView(m_Device->GetDevice(), &viewInfo, nullptr,
                        &m_CubeImageView) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create cube shadow map image view!");
  }
}

void PointShadowMap::CreateFaceImageViews() {
  for (uint32_t i = 0; i < NUM_FACES; ++i) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_CubeImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = i;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device->GetDevice(), &viewInfo, nullptr,
                          &m_FaceImageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create cube face image view!");
    }
  }
}

void PointShadowMap::CreateSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.compareEnable = VK_FALSE; // We'll do manual comparison in shader
  samplerInfo.compareOp = VK_COMPARE_OP_LESS;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;

  if (vkCreateSampler(m_Device->GetDevice(), &samplerInfo, nullptr,
                      &m_Sampler) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create shadow map sampler!");
  }
}

void PointShadowMap::CreateRenderPass() {
  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = VK_FORMAT_D32_SFLOAT;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 0;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 0;
  subpass.pColorAttachments = nullptr;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  // Subpass dependencies
  std::array<VkSubpassDependency, 2> dependencies{};

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &depthAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  if (vkCreateRenderPass(m_Device->GetDevice(), &renderPassInfo, nullptr,
                         &m_RenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create shadow render pass!");
  }
}

void PointShadowMap::CreateFramebuffers() {
  for (uint32_t i = 0; i < NUM_FACES; ++i) {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_RenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &m_FaceImageViews[i];
    framebufferInfo.width = m_Resolution;
    framebufferInfo.height = m_Resolution;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_Device->GetDevice(), &framebufferInfo, nullptr,
                            &m_Framebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create shadow framebuffer!");
    }
  }
}

glm::mat4 PointShadowMap::GetFaceViewMatrix(const glm::vec3 &lightPos,
                                            uint32_t face) const {
  // Cube face directions (standard cube map order):
  // 0: +X, 1: -X, 2: +Y, 3: -Y, 4: +Z, 5: -Z
  static const glm::vec3 targets[6] = {
      glm::vec3(-1.0f, 0.0f, 0.0f),  // +X
      glm::vec3(1.0f, 0.0f, 0.0f), // -X
      glm::vec3(0.0f, 1.0f, 0.0f),  // +Y
      glm::vec3(0.0f, -1.0f, 0.0f), // -Y
      glm::vec3(0.0f, 0.0f, 1.0f),  // +Z
      glm::vec3(0.0f, 0.0f, -1.0f)  // -Z
  };

  static const glm::vec3 ups[6] = {
      glm::vec3(0.0f, 1.0f, 0.0f), // +X
      glm::vec3(0.0f, 1.0f, 0.0f), // -X
      glm::vec3(0.0f, 0.0f, 1.0f),  // +Y
      glm::vec3(0.0f, 0.0f, -1.0f), // -Y
      glm::vec3(0.0f, 1.0f, 0.0f), // +Z
      glm::vec3(0.0f, 1.0f, 0.0f)  // -Z
  };

  return glm::lookAt(lightPos, lightPos + targets[face], ups[face]);
}

glm::mat4 PointShadowMap::GetProjectionMatrix() const {
  // 90 degree FOV to cover each cube face
  auto proj =
      glm::perspective(glm::radians(90.0f), 1.0f, m_NearPlane, m_FarPlane);

  // Vulkan clip space has inverted Y (Top is -1).
  // GLM produces GL clip space (Top is +1).
  // We MUST flip Y to align World Up (+Y) with Screen Up (Top/-1).
  proj[1][1] *= -1;

  return proj;
}

glm::mat4 PointShadowMap::GetLightSpaceMatrix(const glm::vec3 &lightPos,
                                              uint32_t face) const {
  glm::mat4 view = GetFaceViewMatrix(lightPos, face);
  glm::mat4 proj = GetProjectionMatrix();

  return GetProjectionMatrix() * GetFaceViewMatrix(lightPos, face);
}

VkImageView PointShadowMap::GetFaceImageView(uint32_t face) const {
  if (face >= NUM_FACES) {
    return VK_NULL_HANDLE;
  }
  return m_FaceImageViews[face];
}

VkFramebuffer PointShadowMap::GetFramebuffer(uint32_t face) const {
  if (face >= NUM_FACES) {
    return VK_NULL_HANDLE;
  }
  return m_Framebuffers[face];
}

} // namespace Quantum
