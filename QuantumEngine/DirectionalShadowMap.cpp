#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "DirectionalShadowMap.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Quantum {

DirectionalShadowMap::~DirectionalShadowMap() { Shutdown(); }

void DirectionalShadowMap::Initialize(Vivid::VividDevice *device,
                                      uint32_t resolution) {
  if (m_Initialized)
    Shutdown();

  m_Device = device;
  m_Resolution = resolution;

  CreateImage();
  CreateImageView();
  CreateSampler();
  CreateRenderPass();
  CreateFramebuffer();

  m_Initialized = true;
  std::cout << "[DirectionalShadowMap] Initialized with resolution "
            << resolution << std::endl;
}

void DirectionalShadowMap::Shutdown() {
  if (!m_Initialized)
    return;

  VkDevice device = m_Device->GetDevice();

  if (m_Framebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
  if (m_RenderPass != VK_NULL_HANDLE)
    vkDestroyRenderPass(device, m_RenderPass, nullptr);
  if (m_Sampler != VK_NULL_HANDLE)
    vkDestroySampler(device, m_Sampler, nullptr);
  if (m_ImageView != VK_NULL_HANDLE)
    vkDestroyImageView(device, m_ImageView, nullptr);
  if (m_Image != VK_NULL_HANDLE)
    vkDestroyImage(device, m_Image, nullptr);
  if (m_Memory != VK_NULL_HANDLE)
    vkFreeMemory(device, m_Memory, nullptr);

  m_Framebuffer = VK_NULL_HANDLE;
  m_RenderPass = VK_NULL_HANDLE;
  m_Sampler = VK_NULL_HANDLE;
  m_ImageView = VK_NULL_HANDLE;
  m_Image = VK_NULL_HANDLE;
  m_Memory = VK_NULL_HANDLE;

  m_Initialized = false;
}

void DirectionalShadowMap::CreateImage() {
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

  m_Device->CreateImage(
      m_Resolution, m_Resolution, depthFormat, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Image, m_Memory);
}

void DirectionalShadowMap::CreateImageView() {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_Image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_D32_SFLOAT;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_Device->GetDevice(), &viewInfo, nullptr,
                        &m_ImageView) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create shadow map image view!");
  }
}

void DirectionalShadowMap::CreateSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  // Disable comparison sampling - shader does manual depth comparison
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 1.0f;

  if (vkCreateSampler(m_Device->GetDevice(), &samplerInfo, nullptr,
                      &m_Sampler) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create shadow map sampler!");
  }
}

void DirectionalShadowMap::CreateRenderPass() {
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
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  std::array<VkSubpassDependency, 2> dependencies;

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
    throw std::runtime_error("Failed to create shadow map render pass!");
  }
}

void DirectionalShadowMap::CreateFramebuffer() {
  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = m_RenderPass;
  framebufferInfo.attachmentCount = 1;
  framebufferInfo.pAttachments = &m_ImageView;
  framebufferInfo.width = m_Resolution;
  framebufferInfo.height = m_Resolution;
  framebufferInfo.layers = 1;

  if (vkCreateFramebuffer(m_Device->GetDevice(), &framebufferInfo, nullptr,
                          &m_Framebuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create shadow map framebuffer!");
  }
}

glm::mat4
DirectionalShadowMap::GetLightSpaceMatrix(const glm::vec3 &lightDir,
                                          const glm::vec3 &target) const {
  // Shadow area configuration - constant for stable shadows
  const float shadowAreaHalfSize = 20.0f;

  // Center the shadow area at the target position
  glm::vec3 areaCenter = target;

  // Use world's Y-axis as up direction
  glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);

  // CRITICAL: Negate the light direction (matches working reference)
  // lightDir is the direction light is shining; we need direction TO light
  // source
  glm::vec3 negLightDir = -glm::normalize(lightDir);

  // Handle case where light is pointing straight up/down
  if (std::abs(negLightDir.y) > 0.999f) {
    upVector = glm::vec3(0.0f, 0.0f, 1.0f);
  }

  // Position the "light" far back from the scene center
  // negLightDir is the direction TO the light, so we ADD it to the center
  glm::vec3 lightPos = areaCenter + negLightDir * 80.0f;

  // Create view matrix looking from light position toward scene center
  glm::mat4 lightView = glm::lookAt(lightPos, areaCenter, upVector);

  // Calculate dynamic near/far planes from scene bounds corners for tight fit
  std::vector<glm::vec4> worldSpaceCorners;
  worldSpaceCorners.reserve(8);
  worldSpaceCorners.push_back(glm::vec4(
      areaCenter.x - shadowAreaHalfSize, areaCenter.y - shadowAreaHalfSize,
      areaCenter.z - shadowAreaHalfSize, 1.0f));
  worldSpaceCorners.push_back(glm::vec4(
      areaCenter.x + shadowAreaHalfSize, areaCenter.y - shadowAreaHalfSize,
      areaCenter.z - shadowAreaHalfSize, 1.0f));
  worldSpaceCorners.push_back(glm::vec4(
      areaCenter.x + shadowAreaHalfSize, areaCenter.y + shadowAreaHalfSize,
      areaCenter.z - shadowAreaHalfSize, 1.0f));
  worldSpaceCorners.push_back(glm::vec4(
      areaCenter.x - shadowAreaHalfSize, areaCenter.y + shadowAreaHalfSize,
      areaCenter.z - shadowAreaHalfSize, 1.0f));
  worldSpaceCorners.push_back(glm::vec4(
      areaCenter.x - shadowAreaHalfSize, areaCenter.y - shadowAreaHalfSize,
      areaCenter.z + shadowAreaHalfSize, 1.0f));
  worldSpaceCorners.push_back(glm::vec4(
      areaCenter.x + shadowAreaHalfSize, areaCenter.y - shadowAreaHalfSize,
      areaCenter.z + shadowAreaHalfSize, 1.0f));
  worldSpaceCorners.push_back(glm::vec4(
      areaCenter.x + shadowAreaHalfSize, areaCenter.y + shadowAreaHalfSize,
      areaCenter.z + shadowAreaHalfSize, 1.0f));
  worldSpaceCorners.push_back(glm::vec4(
      areaCenter.x - shadowAreaHalfSize, areaCenter.y + shadowAreaHalfSize,
      areaCenter.z + shadowAreaHalfSize, 1.0f));

  float minZ = std::numeric_limits<float>::max();
  float maxZ = std::numeric_limits<float>::lowest();

  for (const auto &corner : worldSpaceCorners) {
    glm::vec4 trf = lightView * corner;
    minZ = std::min(minZ, trf.z);
    maxZ = std::max(maxZ, trf.z);
  }

  // Orthographic projection with stable width/height and dynamic near/far
  // VULKAN NOTE: GLM_FORCE_DEPTH_ZERO_TO_ONE expects positive distances for
  // zNear and zFar. Our view space Z values are negative (eye looks towards
  // -Z). Thus, distances are -maxZ (near) and -minZ (far). We add a bit of
  // padding (10.0f) to avoid clipping artifacts at the edges.
  glm::mat4 lightProjection =
      glm::ortho(-shadowAreaHalfSize, shadowAreaHalfSize, -shadowAreaHalfSize,
                 shadowAreaHalfSize, -maxZ - 10.0f, -minZ + 10.0f);

  // Vulkan clip space has Y down, so flip it here
  lightProjection[1][1] *= -1;

  return lightProjection * lightView;
}

} // namespace Quantum
