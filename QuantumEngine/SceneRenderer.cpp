#include "SceneRenderer.h"
#include "CameraNode.h"
#include "Draw2D.h"
#include "GraphNode.h"
#include "LightNode.h"
#include "Material.h"
#include "Mesh3D.h"
#include "RenderingPipelines.h"
#include "Texture2D.h"
#include "VividApplication.h"
#include "VividPipeline.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "pch.h"
#include <array>
#include <iostream>

namespace Quantum {

// Uniform buffer structure for MVP matrices and lighting
// MUST match PLPBR.frag layout exactly!
struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
  glm::vec3 viewPos;
  float padding;
  glm::vec3 lightPos;
  float padding2;
  glm::vec3 lightColor;
  float lightRange; // 0 = infinite range, otherwise max light distance
};

SceneRenderer::SceneRenderer(Vivid::VividDevice *device,
                             Vivid::VividRenderer *renderer)
    : m_Device(device), m_Renderer(renderer) {
  std::cout << "[SceneRenderer] Constructor called" << std::endl;
  if (m_Device) {
    std::cout << "[SceneRenderer] Device pointer is valid" << std::endl;
  } else {
    std::cerr << "[SceneRenderer] ERROR: Device pointer is NULL!" << std::endl;
  }
  if (m_Renderer) {
    std::cout << "[SceneRenderer] Renderer pointer is valid" << std::endl;
  } else {
    std::cerr << "[SceneRenderer] ERROR: Renderer pointer is NULL!"
              << std::endl;
  }
}

SceneRenderer::~SceneRenderer() {
  std::cout << "[SceneRenderer] Destructor called" << std::endl;
  Shutdown();
}

void SceneRenderer::Initialize() {
  std::cout << "[SceneRenderer] Initialize() called" << std::endl;

  if (m_Initialized) {
    std::cout << "[SceneRenderer] Already initialized, skipping" << std::endl;
    return;
  }

  std::cout << "[SceneRenderer] Creating descriptor set layout..." << std::endl;
  CreateDescriptorSetLayout();
  std::cout << "[SceneRenderer] Descriptor set layout created successfully"
            << std::endl;

  std::cout << "[SceneRenderer] Creating uniform buffer..." << std::endl;
  CreateUniformBuffer();
  std::cout << "[SceneRenderer] Uniform buffer created successfully"
            << std::endl;

  // Create default white texture
  std::cout << "[SceneRenderer] Creating default texture..." << std::endl;
  uint32_t white = 0xFFFFFFFF;
  m_DefaultTexture = std::make_shared<Vivid::Texture2D>(
      m_Device, reinterpret_cast<unsigned char *>(&white), 1, 1, 4);
  std::cout << "[SceneRenderer] Default texture created" << std::endl;

  // Initialize RenderingPipelines with our descriptor layout
  std::cout << "[SceneRenderer] Initializing RenderingPipelines..."
            << std::endl;
  RenderingPipelines::Get().Initialize(m_Device, m_Renderer->GetRenderPass(),
                                       m_DescriptorSetLayout);
  std::cout << "[SceneRenderer] RenderingPipelines initialized" << std::endl;

  // Register the default PLSimple pipeline as a 3D mesh pipeline
  std::cout << "[SceneRenderer] Registering PLSimple pipeline with shaders:"
            << std::endl;
  std::cout << "[SceneRenderer]   Vertex:   engine/shaders/PLSimple.vert.spv"
            << std::endl;
  std::cout << "[SceneRenderer]   Fragment: engine/shaders/PLSimple.frag.spv"
            << std::endl;
  std::cout << "[SceneRenderer]   Type: Mesh3D" << std::endl;

  RenderingPipelines::Get().RegisterPipeline(
      "PLSimple", "engine/shaders/PLSimple.vert.spv",
      "engine/shaders/PLSimple.frag.spv", Vivid::BlendConfig{},
      Vivid::PipelineType::Mesh3D);

  if (RenderingPipelines::Get().HasPipeline("PLSimple")) {
    std::cout << "[SceneRenderer] PLSimple pipeline registered successfully"
              << std::endl;
  } else {
    std::cerr << "[SceneRenderer] ERROR: Failed to register PLSimple pipeline!"
              << std::endl;
  }

  // Verify pipeline was created
  auto *pipeline = RenderingPipelines::Get().GetPipeline("PLSimple");
  if (pipeline) {
    std::cout << "[SceneRenderer] PLSimple pipeline verified - GetPipeline "
                 "returned valid pointer"
              << std::endl;
  } else {
    std::cerr << "[SceneRenderer] ERROR: PLSimple pipeline is NULL after "
                 "registration!"
              << std::endl;
  }

  // Register the PLPBR pipeline
  std::cout << "[SceneRenderer] Registering PLPBR pipeline with shaders:"
            << std::endl;
  std::cout << "[SceneRenderer]   Vertex:   engine/shaders/PLPBR.vert.spv"
            << std::endl;
  std::cout << "[SceneRenderer]   Fragment: engine/shaders/PLPBR.frag.spv"
            << std::endl;
  std::cout << "[SceneRenderer]   Type: Mesh3D" << std::endl;

  // Opaque blend config for first light (no blending, just overwrite)
  Vivid::BlendConfig opaqueConfig;
  opaqueConfig.blendEnable = VK_FALSE; // Disable blending for opaque pass
  opaqueConfig.depthCompareOp = VK_COMPARE_OP_LESS;
  opaqueConfig.depthWriteEnable = VK_TRUE;

  RenderingPipelines::Get().RegisterPipeline(
      "PLPBR", "engine/shaders/PLPBR.vert.spv", "engine/shaders/PLPBR.frag.spv",
      opaqueConfig, Vivid::PipelineType::Mesh3D);

  if (RenderingPipelines::Get().HasPipeline("PLPBR")) {
    std::cout << "[SceneRenderer] PLPBR pipeline registered successfully"
              << std::endl;
  } else {
    std::cerr << "[SceneRenderer] ERROR: Failed to register PLPBR pipeline!"
              << std::endl;
  }

  // Register additive pipeline for additional lights
  Vivid::BlendConfig additiveConfig;
  additiveConfig.blendEnable = VK_TRUE;
  additiveConfig.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  additiveConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  additiveConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  additiveConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

  // Depth testing for additive pass - use LESS_OR_EQUAL for proper occlusion
  additiveConfig.depthTestEnable = VK_TRUE;
  // CRITICAL: Use LESS_OR_EQUAL, not EQUAL - EQUAL is too strict for FP
  // precision
  additiveConfig.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  additiveConfig.depthWriteEnable = VK_FALSE;

  // Depth bias to push additive fragments toward camera to pass depth test
  additiveConfig.depthBiasEnable = VK_TRUE;
  additiveConfig.depthBiasConstantFactor = -1.0f;
  additiveConfig.depthBiasSlopeFactor = -1.0f;

  RenderingPipelines::Get().RegisterPipeline(
      "PLPBR_Additive", "engine/shaders/PLPBR.vert.spv",
      "engine/shaders/PLPBR.frag.spv", additiveConfig,
      Vivid::PipelineType::Mesh3D);

  std::cout
      << "[SceneRenderer] PLPBR_Additive pipeline registered for multi-light"
      << std::endl;

  // Initialize shadow mapping resources
  InitializeShadowResources();

  m_Initialized = true;
  std::cout << "[SceneRenderer] Initialization complete" << std::endl;
}

void SceneRenderer::Shutdown() {
  std::cout << "[SceneRenderer] Shutdown() called" << std::endl;

  if (!m_Initialized) {
    std::cout << "[SceneRenderer] Not initialized, nothing to shutdown"
              << std::endl;
    return;
  }

  if (m_Device) {
    std::cout << "[SceneRenderer] Waiting for device idle..." << std::endl;
    vkDeviceWaitIdle(m_Device->GetDevice());
  }

  std::cout << "[SceneRenderer] Resetting scene graph..." << std::endl;
  m_SceneGraph.reset();

  // Cleanup shadow resources
  std::cout << "[SceneRenderer] Cleaning up shadow resources..." << std::endl;
  m_ShadowPipeline.reset();
  if (m_ShadowMap) {
    m_ShadowMap->Shutdown();
    m_ShadowMap.reset();
  }

  std::cout << "[SceneRenderer] Resetting uniform buffers..." << std::endl;
  for (auto &buffer : m_UniformBuffers) {
    buffer.reset();
  }

  if (m_DescriptorPool != VK_NULL_HANDLE && m_Device) {
    std::cout << "[SceneRenderer] Destroying descriptor pool..." << std::endl;
    vkDestroyDescriptorPool(m_Device->GetDevice(), m_DescriptorPool, nullptr);
    m_DescriptorPool = VK_NULL_HANDLE;
  }

  if (m_DescriptorSetLayout != VK_NULL_HANDLE && m_Device) {
    std::cout << "[SceneRenderer] Destroying descriptor set layout..."
              << std::endl;
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_DescriptorSetLayout,
                                 nullptr);
    m_DescriptorSetLayout = VK_NULL_HANDLE;
  }

  std::cout << "[SceneRenderer] Shutting down RenderingPipelines..."
            << std::endl;
  RenderingPipelines::Get().Shutdown();

  m_Initialized = false;
  std::cout << "[SceneRenderer] Shutdown complete" << std::endl;
}

void SceneRenderer::SetSceneGraph(std::shared_ptr<SceneGraph> sceneGraph) {
  std::cout << "[SceneRenderer] SetSceneGraph() called" << std::endl;

  m_SceneGraph = sceneGraph;

  if (m_SceneGraph) {
    std::cout << "[SceneRenderer] Scene graph assigned successfully"
              << std::endl;
    std::cout << "[SceneRenderer] Scene has " << m_SceneGraph->GetNodeCount()
              << " nodes" << std::endl;
  } else {
    std::cerr << "[SceneRenderer] WARNING: Scene graph is NULL!" << std::endl;
  }

  // Create descriptor pool and sets when scene is set
  if (m_SceneGraph && m_DescriptorPool == VK_NULL_HANDLE) {
    std::cout << "[SceneRenderer] Creating descriptor pool..." << std::endl;
    CreateDescriptorPool();
    std::cout << "[SceneRenderer] Creating descriptor sets..." << std::endl;
    CreateDescriptorSets();
    std::cout << "[SceneRenderer] Descriptor resources created" << std::endl;

    // Update descriptor sets with first material's textures
    // (This must happen BEFORE command buffer recording starts)
    std::cout
        << "[SceneRenderer] Looking for first material to bind textures..."
        << std::endl;
    if (m_SceneGraph->GetRoot()) {
      UpdateFirstMaterialTextures(m_SceneGraph->GetRoot());
      // Create descriptor sets for all materials
      RefreshMaterialTextures();
    }
  }
}

void SceneRenderer::CreateDescriptorSetLayout() {
  std::cout << "[SceneRenderer] CreateDescriptorSetLayout() started"
            << std::endl;

  // Binding 0: Uniform buffer (vertex shader)
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT |
      VK_SHADER_STAGE_FRAGMENT_BIT; // Both shaders need UBO
  uboLayoutBinding.pImmutableSamplers = nullptr;

  // Binding 1: Albedo texture
  VkDescriptorSetLayoutBinding albedoBinding{};
  albedoBinding.binding = 1;
  albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  albedoBinding.descriptorCount = 1;
  albedoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  albedoBinding.pImmutableSamplers = nullptr;

  // Binding 2: Normal texture
  VkDescriptorSetLayoutBinding normalBinding{};
  normalBinding.binding = 2;
  normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  normalBinding.descriptorCount = 1;
  normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  normalBinding.pImmutableSamplers = nullptr;

  // Binding 3: Metallic texture
  VkDescriptorSetLayoutBinding metallicBinding{};
  metallicBinding.binding = 3;
  metallicBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  metallicBinding.descriptorCount = 1;
  metallicBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  metallicBinding.pImmutableSamplers = nullptr;

  // Binding 4: Roughness texture
  VkDescriptorSetLayoutBinding roughnessBinding{};
  roughnessBinding.binding = 4;
  roughnessBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  roughnessBinding.descriptorCount = 1;
  roughnessBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  roughnessBinding.pImmutableSamplers = nullptr;

  roughnessBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  roughnessBinding.pImmutableSamplers = nullptr;

  // Binding 5: Shadow cube map
  VkDescriptorSetLayoutBinding shadowBinding{};
  shadowBinding.binding = 5;
  shadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  shadowBinding.descriptorCount = 1;
  shadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  shadowBinding.pImmutableSamplers = nullptr;

  std::array<VkDescriptorSetLayoutBinding, 6> bindings = {
      uboLayoutBinding, albedoBinding,    normalBinding,
      metallicBinding,  roughnessBinding, shadowBinding};

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  VkResult result = vkCreateDescriptorSetLayout(
      m_Device->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout);
  if (result != VK_SUCCESS) {
    std::cerr << "[SceneRenderer] ERROR: Failed to create descriptor set "
                 "layout! VkResult: "
              << result << std::endl;
    throw std::runtime_error("Failed to create descriptor set layout!");
  }

  std::cout
      << "[SceneRenderer] CreateDescriptorSetLayout() completed successfully"
      << std::endl;
}

void SceneRenderer::CreateDescriptorPool() {
  std::cout << "[SceneRenderer] CreateDescriptorPool() started" << std::endl;

  // Increased to support per-material descriptor sets + shadow map
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  poolSizes[0].descriptorCount = 10; // Global UBO + spare
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount =
      500; // 5 textures (4 PBR + 1 shadow) * 100 materials

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 100; // Support up to 100 materials

  VkResult result = vkCreateDescriptorPool(m_Device->GetDevice(), &poolInfo,
                                           nullptr, &m_DescriptorPool);
  if (result != VK_SUCCESS) {
    std::cerr
        << "[SceneRenderer] ERROR: Failed to create descriptor pool! VkResult: "
        << result << std::endl;
    throw std::runtime_error("Failed to create descriptor pool!");
  }

  std::cout << "[SceneRenderer] CreateDescriptorPool() completed successfully"
            << std::endl;
}

void SceneRenderer::CreateDescriptorSets() {
  std::cout << "[SceneRenderer] CreateDescriptorSets() started" << std::endl;

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_DescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_DescriptorSetLayout;

  VkResult result = vkAllocateDescriptorSets(m_Device->GetDevice(), &allocInfo,
                                             &m_DescriptorSet);
  if (result != VK_SUCCESS) {
    std::cerr << "[SceneRenderer] ERROR: Failed to allocate descriptor set! "
                 "VkResult: "
              << result << std::endl;
    throw std::runtime_error("Failed to allocate descriptor set!");
  }

  std::cout << "[SceneRenderer] Descriptor set allocated" << std::endl;

  // Update descriptor set with uniform buffer (binding 0)
  // Note: We use frame 0's buffer initially; the actual buffer used depends on
  // dynamic offset at draw time, and we update descriptor per-frame
  VkDescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = m_UniformBuffers[0]->GetBuffer();
  bufferInfo.offset = 0;
  bufferInfo.range = sizeof(UniformBufferObject);

  VkWriteDescriptorSet uboWrite{};
  uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  uboWrite.dstSet = m_DescriptorSet;
  uboWrite.dstBinding = 0;
  uboWrite.dstArrayElement = 0;
  uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  uboWrite.descriptorCount = 1;
  uboWrite.pBufferInfo = &bufferInfo;

  // Update descriptor set with default texture (bindings 1-4)
  // CRITICAL: Each VkWriteDescriptorSet needs its own VkDescriptorImageInfo
  std::array<VkDescriptorImageInfo, 4> imageInfos;
  for (int i = 0; i < 4; ++i) {
    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[i].imageView = m_DefaultTexture->GetImageView();
    imageInfos[i].sampler = m_DefaultTexture->GetSampler();
  }

  // Albedo (binding 1)
  VkWriteDescriptorSet albedoWrite{};
  albedoWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  albedoWrite.dstSet = m_DescriptorSet;
  albedoWrite.dstBinding = 1;
  albedoWrite.dstArrayElement = 0;
  albedoWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  albedoWrite.descriptorCount = 1;
  albedoWrite.pImageInfo = &imageInfos[0];

  // Normal (binding 2)
  VkWriteDescriptorSet normalWrite{};
  normalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  normalWrite.dstSet = m_DescriptorSet;
  normalWrite.dstBinding = 2;
  normalWrite.dstArrayElement = 0;
  normalWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  normalWrite.descriptorCount = 1;
  normalWrite.pImageInfo = &imageInfos[1];

  // Metallic (binding 3)
  VkWriteDescriptorSet metallicWrite{};
  metallicWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  metallicWrite.dstSet = m_DescriptorSet;
  metallicWrite.dstBinding = 3;
  metallicWrite.dstArrayElement = 0;
  metallicWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  metallicWrite.descriptorCount = 1;
  metallicWrite.pImageInfo = &imageInfos[2];

  // Roughness (binding 4)
  VkWriteDescriptorSet roughnessWrite{};
  roughnessWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  roughnessWrite.dstSet = m_DescriptorSet;
  roughnessWrite.dstBinding = 4;
  roughnessWrite.dstArrayElement = 0;
  roughnessWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  roughnessWrite.descriptorCount = 1;
  roughnessWrite.pImageInfo = &imageInfos[3];

  // AO and Emissive removed

  std::array<VkWriteDescriptorSet, 5> descriptorWrites = {
      uboWrite, albedoWrite, normalWrite, metallicWrite, roughnessWrite};
  vkUpdateDescriptorSets(m_Device->GetDevice(),
                         static_cast<uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(), 0, nullptr);

  std::cout << "[SceneRenderer] CreateDescriptorSets() completed successfully"
            << std::endl;
}

void SceneRenderer::CreateUniformBuffer() {
  std::cout << "[SceneRenderer] CreateUniformBuffer() started" << std::endl;
  std::cout << "[SceneRenderer] UniformBufferObject size: "
            << sizeof(UniformBufferObject) << " bytes" << std::endl;

  // Query minimum uniform buffer offset alignment from device
  VkPhysicalDeviceProperties deviceProps;
  vkGetPhysicalDeviceProperties(m_Device->GetPhysicalDevice(), &deviceProps);
  m_UniformBufferAlignment =
      static_cast<uint32_t>(deviceProps.limits.minUniformBufferOffsetAlignment);

  // Calculate aligned UBO size (must be multiple of alignment)
  m_AlignedUBOSize =
      (sizeof(UniformBufferObject) + m_UniformBufferAlignment - 1) &
      ~(m_UniformBufferAlignment - 1);

  std::cout << "[SceneRenderer] Device min UBO alignment: "
            << m_UniformBufferAlignment << " bytes" << std::endl;
  std::cout << "[SceneRenderer] Aligned UBO size: " << m_AlignedUBOSize
            << " bytes" << std::endl;

  // Create buffer large enough for m_MaxDraws worth of UBO data
  VkDeviceSize totalBufferSize = m_AlignedUBOSize * m_MaxDraws;
  std::cout << "[SceneRenderer] Total uniform buffer size: " << totalBufferSize
            << " bytes (for " << m_MaxDraws << " draws)" << std::endl;

  // Create per-frame UBO buffers to prevent race conditions
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    m_UniformBuffers[i] = std::make_unique<Vivid::VividBuffer>(
        m_Device, totalBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_UniformBuffers[i]->Map();
    std::cout << "[SceneRenderer] Created UBO buffer for frame " << i
              << std::endl;
  }

  std::cout << "[SceneRenderer] CreateUniformBuffer() completed successfully"
            << std::endl;
}

void SceneRenderer::ResizeUniformBuffers(size_t requiredDraws) {
  std::cout << "[SceneRenderer] Resizing UBO from " << m_MaxDraws << " to "
            << requiredDraws << std::endl;

  vkDeviceWaitIdle(m_Device->GetDevice());

  m_MaxDraws = requiredDraws;
  VkDeviceSize totalBufferSize = m_AlignedUBOSize * m_MaxDraws;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    // Determine if we need to explicitly destroy old unique_ptrs?
    // Assignment will destroy old object.
    m_UniformBuffers[i] = std::make_unique<Vivid::VividBuffer>(
        m_Device, totalBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_UniformBuffers[i]->Map();
  }

  // Update main descriptor set (binding 0)
  VkDescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = m_UniformBuffers[0]->GetBuffer();
  bufferInfo.offset = 0;
  bufferInfo.range = sizeof(UniformBufferObject);

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = m_DescriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &bufferInfo;

  vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &descriptorWrite, 0,
                         nullptr);

  std::cerr
      << "[SceneRenderer] WARNING: UBO Resized. Per-material descriptor sets "
         "may be invalid! "
      << "Re-creating materials or using 65536 initial size is recommended."
      << std::endl;
}

void SceneRenderer::RenderScene(VkCommandBuffer cmd, int width, int height) {

  // TODO: Implement per-frame descriptor updates to use multiple UBO buffers
  // For now, force buffer 0 since descriptors are created with buffer 0
  m_CurrentFrameIndex = 0;

  // Only log once per second to avoid spam
  static int frameCount = 0;
  static bool firstFrame = true;
  frameCount++;
  bool shouldLog = firstFrame || (frameCount % 60 == 0);

  if (firstFrame) {
    std::cout << "[SceneRenderer] RenderScene() called for first time"
              << std::endl;
    std::cout << "[SceneRenderer] Viewport size: " << width << "x" << height
              << std::endl;

    if (shouldLog) {
      std::cout << "[SceneRenderer] Frame " << frameCount << ": Rendered "
                << m_RenderNodeCount << " nodes with " << m_RenderMeshCount
                << " meshes" << std::endl;
    }
  }

  // Update view/projection matrices
  // For now, we update the UBO in RenderNode for each mesh,
  // but ideally we should update a global scene UBO once here.

  // Use global frame size if available (as requested by user)
  int frameWidth = Vivid::VividApplication::GetFrameWidth();
  int frameHeight = Vivid::VividApplication::GetFrameHeight();

  if (frameWidth > 0 && frameHeight > 0) {
    width = frameWidth;
    height = frameHeight;
  }

  if (width <= 0 || height <= 0)
    return;

  // Set dynamic viewport
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(width);
  viewport.height = static_cast<float>(height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  // Set dynamic scissor
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Reset counters
  m_RenderNodeCount = 0;
  m_RenderMeshCount = 0;

  // Reset current pipeline state for new frame/command buffer
  m_CurrentPipeline = nullptr;

  // Reset draw index for new frame
  m_CurrentDrawIndex = 0;

  // Render the scene - loop through lights
  if (m_SceneGraph && m_SceneGraph->GetRoot()) {
    const auto &lights = m_SceneGraph->GetLights();
    size_t numLights = lights.empty() ? 1 : lights.size();

    // Check if we need to resize the UBO buffer
    // Calculate total potential draws needed for this frame
    size_t totalMeshes = m_SceneGraph->GetTotalMeshCount();
    size_t neededDraws = totalMeshes * numLights;

    if (neededDraws > m_MaxDraws) {
      size_t newSize =
          std::max(neededDraws + 1000, static_cast<size_t>(m_MaxDraws * 1.5));
      ResizeUniformBuffers(newSize);
    }

    // Normal order
    for (size_t i = 0; i < numLights; ++i) {

      m_CurrentLightIndex = i;
      // Linear accumulation: m_CurrentDrawIndex continues growing across light
      // passes This packs the UBO tightly: [Light0_Draws] [Light1_Draws] ...

      // Set dynamic state INSIDE loop to ensure it persists
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      // Reset pipeline state for each light pass
      m_CurrentPipeline = nullptr;

      RenderNode(cmd, m_SceneGraph->GetRoot(), width, height);
    }
  }

  if (firstFrame) {
    firstFrame = false;
  }
}

void SceneRenderer::RenderNode(VkCommandBuffer cmd, GraphNode *node, int width,
                               int height) {
  if (!node) {
    return;
  }

  m_RenderNodeCount++;

  // Render meshes attached to this node
  if (node->HasMeshes()) {
    // Update animation
    // Update uniform buffer with MVP matrices
    UniformBufferObject ubo{};

    // Just use identity model (ignore node transform for now)
    // Place model 5 units in front of camera
    ubo.model = node->GetWorldMatrix(); // glm::translate(glm::mat4(1.0f),
                                        // glm::vec3(0.0f, 0.0f, -5.0f));

    // Use active camera if available, otherwise fallback to default
    auto camera = m_SceneGraph->GetCurrentCamera();
    if (camera) {
      ubo.view = camera->GetWorldMatrix(); // CameraNode::GetWorldMatrix
                                           // returns View Matrix
    } else {
      // Simple camera at origin looking down negative Z (Fallback)
      ubo.view =
          glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                      glm::vec3(0.0f, 1.0f, 0.0f));
    }

    ubo.proj = glm::perspective(glm::radians(45.0f),
                                (float)width / (float)height, 0.1f, 100.0f);
    ubo.proj[1][1] *= -1; // Flip Y for Vulkan

    // Set lighting data for PBR
    if (camera) {
      ubo.viewPos =
          camera->GetWorldPosition(); // Use inherited GraphNode method
    } else {
      ubo.viewPos = glm::vec3(0.0f, 0.0f, 0.0f);
    }
    ubo.padding = 0.0f;

    // Use current light based on m_CurrentLightIndex (set by RenderScene loop)
    if (m_SceneGraph) {
      const auto &lights = m_SceneGraph->GetLights();
      if (m_CurrentLightIndex < lights.size()) {
        auto light = lights[m_CurrentLightIndex];
        ubo.lightPos = light->GetWorldPosition();
        ubo.lightColor = light->GetColor();
        ubo.lightRange = light->GetRange();
      } else {
        // Default fallbacks if no lights
        ubo.lightPos = glm::vec3(5.0f, 5.0f, 5.0f);
        ubo.lightColor = glm::vec3(150.0f, 150.0f, 150.0f);
        ubo.lightRange = 150.0f;
      }
    } else {
      ubo.lightPos = glm::vec3(5.0f, 5.0f, 5.0f);
      ubo.lightColor = glm::vec3(150.0f, 150.0f, 150.0f);
      ubo.lightRange = 150.0f;
    }

    // Debug output to verify light position sync
    // std::cout << "Light Pos: " << ubo.lightPos.x << "," << ubo.lightPos.y <<
    // "," << ubo.lightPos.z << std::endl;

    ubo.padding2 = 0.0f;

    // Debug: Log light data for each pass (temporary)

    // DYNAMIC BUFFER STRATEGY:
    // We strictly use linear indexing. Check capacity.
    if (m_CurrentDrawIndex >= m_MaxDraws) {
      // Buffer full, cannot write debug info or draw
      // (This check should ideally be in the mesh loop for safety, but we do it
      // here for log consistency)
    }
    VkDeviceSize drawOffset = m_CurrentDrawIndex * m_AlignedUBOSize;

    // Redundant UBO write removed - we write inside the mesh loop
    // But we still calculate drawOffset for debug logging

    // DEBUG: Track every node/light/draw combination
    static int debugCounter = 0;
    if (debugCounter < 200) {
      std::cout << "[UBO] Node=" << node->GetName()
                << " LightIdx=" << m_CurrentLightIndex
                << " DrawIdx=" << m_CurrentDrawIndex << " Offset=" << drawOffset
                << " LightColor=(" << ubo.lightColor.x << ","
                << ubo.lightColor.y << "," << ubo.lightColor.z << ")"
                << std::endl;
      debugCounter++;
    }

    // Render each mesh
    for (const auto &mesh : node->GetMeshes()) {
      if (mesh) {
        if (mesh->IsFinalized()) {
          // Get the pipeline and texture from the mesh's material
          Vivid::VividPipeline *meshPipeline = nullptr;
          Vivid::Texture2D *albedoTexture = nullptr;
          auto material = mesh->GetMaterial();
          if (material) {
            meshPipeline = material->GetPipeline();
            auto albedoTex = material->GetAlbedoTexture();
            if (albedoTex) {
              albedoTexture = albedoTex.get();
            }
          }

          // Fall back to default PLSimple if no material or pipeline
          if (!meshPipeline) {
            meshPipeline = RenderingPipelines::Get().GetPipeline("PLSimple");
          }

          // DYNAMIC PIPELINE SWITCH FOR MULTI-LIGHT PASS
          // CRITICAL: This must happen AFTER fallback, so ALL meshes get
          // additive
          if (m_CurrentLightIndex > 0 && meshPipeline) {
            // Switch to additive pipeline for additional lights
            meshPipeline = Quantum::RenderingPipelines::Get().GetPipeline(
                "PLPBR_Additive");

            if (!meshPipeline) {
              std::cerr << "[SceneRenderer] ERROR: Failed to get "
                           "PLPBR_Additive pipeline! Fallback to Opaque."
                        << std::endl;
              if (material) {
                meshPipeline = material->GetPipeline();
              } else {
                meshPipeline =
                    RenderingPipelines::Get().GetPipeline("PLSimple");
              }
            } else {
              static bool loggedSwitch = false;
              if (!loggedSwitch && m_CurrentLightIndex == 1) {

                loggedSwitch = true;
              }
            }
          }

          // Bind pipeline if it changed
          if (meshPipeline && meshPipeline != m_CurrentPipeline) {
            // DEBUG: Log pipeline switch
            static int pipelineLogCount = 0;
            if (pipelineLogCount < 50) {
              std::cout << "[PIPELINE] Node=" << node->GetName()
                        << " LightIdx=" << m_CurrentLightIndex
                        << " SWITCHING pipeline (prev="
                        << (m_CurrentPipeline ? "set" : "null") << ") to "
                        << (m_CurrentLightIndex > 0 ? "ADDITIVE" : "OPAQUE")
                        << std::endl;
              pipelineLogCount++;
            }
            m_CurrentPipeline = meshPipeline;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              meshPipeline->GetPipeline());
          }

          // Safety check: ensure we have a bound pipeline
          if (!m_CurrentPipeline) {
            std::cerr << "[SceneRenderer] ERROR: No pipeline bound!"
                      << std::endl;
            continue;
          }

          // Bind the material's descriptor set (per-material textures)
          VkDescriptorSet descriptorSetToBind =
              m_DescriptorSet; // Default fallback
          bool usingMaterialDescriptor = false;
          // Use per-material descriptor set if available
          if (material && material->HasDescriptorSet()) {
            descriptorSetToBind = material->GetDescriptorSet();
            usingMaterialDescriptor = true;
          }

          if (descriptorSetToBind != VK_NULL_HANDLE) {
            // Perform Dynamic Buffer Safety Check
            if (m_CurrentDrawIndex >= m_MaxDraws) {
              // Optional: Trigger resize here? Or just skip.
              // For now, skip to prevent crash.
              std::cerr << "[SceneRenderer] ERROR: Max draws exceeded ("
                        << m_MaxDraws << ")! "
                        << "Increase m_MaxDraws or implement dynamic resize."
                        << std::endl;
              continue;
            }

            // Use direct linear index for dynamic offset
            uint32_t dynamicOffset =
                static_cast<uint32_t>(m_CurrentDrawIndex * m_AlignedUBOSize);

            // CRITIAL FIX: Upload the UBO data to the mapped buffer!
            // Without this, we are drawing with garbage/zeros.
            void *mappedData =
                m_UniformBuffers[m_CurrentFrameIndex]->GetMappedMemory();
            if (mappedData) {
              char *dest = (char *)mappedData + dynamicOffset;
              memcpy(dest, &ubo, sizeof(UniformBufferObject));
            } else {
              std::cerr << "ERROR: UBO not mapped!" << std::endl;
            }

            // DEBUG LOGGING - Enhanced to track node/pipeline/blend + LIGHT
            // COLOR + DESCRIPTOR SET TYPE

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    meshPipeline->GetPipelineLayout(), 0, 1,
                                    &descriptorSetToBind, 1, &dynamicOffset);
          } else {
            std::cerr << "[SceneRenderer] ERROR: Descriptor set is null!"
                      << std::endl;
            continue;
          }

          mesh->Bind(cmd);
          uint32_t indexCount = static_cast<uint32_t>(mesh->GetIndexCount());
          if (indexCount > 0) {
            // DEBUG: Log at actual draw time
            static int drawLogCount = 0;
            if (drawLogCount < 200) {
              uint32_t currentOffset =
                  static_cast<uint32_t>(m_CurrentDrawIndex * m_AlignedUBOSize);
              std::cout << "[DRAW] Node=" << node->GetName()
                        << " LightIdx=" << m_CurrentLightIndex
                        << " DrawIdx=" << m_CurrentDrawIndex
                        << " DynOffset=" << currentOffset
                        << " IndexCount=" << indexCount << std::endl;
              drawLogCount++;
            }
            vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
            m_RenderMeshCount++;
          } else {
            int b = 5;
          }

          // Increment draw index for next draw call
          // We increment per valid draw to ensure unique UBO slots
          m_CurrentDrawIndex++;
        }
      } else {
      }
    }
  }

  // Recursively render children
  for (const auto &child : node->GetChildren()) {
    RenderNode(cmd, child.get(), width, height);
  }
}

void SceneRenderer::UpdateTextureDescriptor(Vivid::Texture2D *texture) {
  if (!texture || texture == m_CurrentTexture) {
    return; // No change needed
  }

  m_CurrentTexture = texture;

  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = texture->GetImageView();
  imageInfo.sampler = texture->GetSampler();

  VkWriteDescriptorSet textureWrite{};
  textureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  textureWrite.dstSet = m_DescriptorSet;
  textureWrite.dstBinding = 1;
  textureWrite.dstArrayElement = 0;
  textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  textureWrite.descriptorCount = 1;
  textureWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &textureWrite, 0, nullptr);
}

void SceneRenderer::UpdatePBRTextures(Material *material) {
  if (!material) {
    return;
  }

  // Get all PBR textures from material (fallback to default)
  auto albedo = material->GetAlbedoTexture();
  auto normal = material->GetNormalTexture();
  auto metallic = material->GetMetallicTexture();
  auto roughness = material->GetRoughnessTexture();
  auto ao = material->GetAOTexture();
  auto emissive = material->GetEmissiveTexture();

  // Use default texture if material texture is missing
  Vivid::Texture2D *albedoTex = albedo ? albedo.get() : m_DefaultTexture.get();
  Vivid::Texture2D *normalTex = normal ? normal.get() : m_DefaultTexture.get();
  Vivid::Texture2D *metallicTex =
      metallic ? metallic.get() : m_DefaultTexture.get();
  Vivid::Texture2D *roughnessTex =
      roughness ? roughness.get() : m_DefaultTexture.get();
  Vivid::Texture2D *aoTex = ao ? ao.get() : m_DefaultTexture.get();
  Vivid::Texture2D *emissiveTex =
      emissive ? emissive.get() : m_DefaultTexture.get();

  // Safety check: ensure default texture exists
  if (!m_DefaultTexture) {
    std::cerr << "[SceneRenderer] ERROR: Default texture is null!" << std::endl;
    return;
  }

  // Additional safety check: verify all pointers are valid
  if (!albedoTex || !normalTex || !metallicTex || !roughnessTex || !aoTex ||
      !emissiveTex) {
    std::cerr << "[SceneRenderer] ERROR: One or more texture pointers are null!"
              << std::endl;
    return;
  }

  // Prepare image infos for all 4 textures
  std::array<VkDescriptorImageInfo, 4> imageInfos;

  std::cout << "[SceneRenderer] Getting Albedo ImageView and Sampler..."
            << std::endl;
  imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfos[0].imageView = albedoTex->GetImageView();
  imageInfos[0].sampler = albedoTex->GetSampler();
  std::cout << "[SceneRenderer]   ImageView: "
            << (void *)imageInfos[0].imageView
            << ", Sampler: " << (void *)imageInfos[0].sampler << std::endl;

  std::cout << "[SceneRenderer] Getting Normal ImageView and Sampler..."
            << std::endl;
  imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfos[1].imageView = normalTex->GetImageView();
  imageInfos[1].sampler = normalTex->GetSampler();
  std::cout << "[SceneRenderer]   ImageView: "
            << (void *)imageInfos[1].imageView
            << ", Sampler: " << (void *)imageInfos[1].sampler << std::endl;

  std::cout << "[SceneRenderer] Getting Metallic ImageView and Sampler..."
            << std::endl;
  imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfos[2].imageView = metallicTex->GetImageView();
  imageInfos[2].sampler = metallicTex->GetSampler();
  std::cout << "[SceneRenderer]   ImageView: "
            << (void *)imageInfos[2].imageView
            << ", Sampler: " << (void *)imageInfos[2].sampler << std::endl;

  std::cout << "[SceneRenderer] Getting Roughness ImageView and Sampler..."
            << std::endl;
  imageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfos[3].imageView = roughnessTex->GetImageView();
  imageInfos[3].sampler = roughnessTex->GetSampler();
  std::cout << "[SceneRenderer]   ImageView: "
            << (void *)imageInfos[3].imageView
            << ", Sampler: " << (void *)imageInfos[3].sampler << std::endl;

  // Update all 4 texture bindings
  std::cout << "[SceneRenderer] Building descriptor writes array..."
            << std::endl;
  std::array<VkWriteDescriptorSet, 4> writes{}; // Zero-initialize all fields
  for (int i = 0; i < 4; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].pNext = nullptr; // Explicitly set pNext
    writes[i].dstSet = m_DescriptorSet;
    writes[i].dstBinding = 1 + i; // Bindings 1-5
    writes[i].dstArrayElement = 0;
    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[i].descriptorCount = 1;
    writes[i].pBufferInfo = nullptr; // Not used for image descriptors
    writes[i].pImageInfo = &imageInfos[i];
    writes[i].pTexelBufferView = nullptr; // Not used
  }

  std::cout << "[SceneRenderer] About to call vkUpdateDescriptorSets, "
               "DescriptorSet: "
            << (void *)m_DescriptorSet << std::endl;

  vkUpdateDescriptorSets(m_Device->GetDevice(),
                         static_cast<uint32_t>(writes.size()), writes.data(), 0,
                         nullptr);

  std::cout << "[SceneRenderer] vkUpdateDescriptorSets completed successfully!"
            << std::endl;
}

void SceneRenderer::UpdateFirstMaterialTextures(GraphNode *node) {
  if (!node)
    return;

  // First pass: Look for a material WITH an albedo texture
  for (const auto &mesh : node->GetMeshes()) {
    if (mesh) {
      auto material = mesh->GetMaterial();
      if (material && material->GetAlbedoTexture()) {
        std::cout << "[SceneRenderer] Found material with textures: "
                  << material->GetName() << std::endl;
        UpdatePBRTextures(material.get());
        return; // Use this material's textures
      }
    }
  }

  // Second pass: If no material with textures found at this level, check
  // children
  for (const auto &child : node->GetChildren()) {
    // Check if child has a material with textures before recursing
    for (const auto &mesh : child->GetMeshes()) {
      if (mesh) {
        auto material = mesh->GetMaterial();
        if (material && material->GetAlbedoTexture()) {
          std::cout << "[SceneRenderer] Found material with textures in child: "
                    << material->GetName() << std::endl;
          UpdatePBRTextures(material.get());
          return;
        }
      }
    }
  }

  // Recursively check children if no material with textures found
  for (const auto &child : node->GetChildren()) {
    UpdateFirstMaterialTextures(child.get());
  }
}

void SceneRenderer::RefreshMaterialTextures() {
  std::cout << "[SceneRenderer] RefreshMaterialTextures: Creating per-material "
               "descriptor sets..."
            << std::endl;

  if (!m_SceneGraph || !m_SceneGraph->GetRoot()) {
    std::cout << "[SceneRenderer] RefreshMaterialTextures: No scene graph"
              << std::endl;
    return;
  }

  // Wait for GPU to be idle before updating descriptors
  vkDeviceWaitIdle(m_Device->GetDevice());

  // Create descriptor sets for all materials in the scene
  CreateMaterialDescriptorSetsRecursive(m_SceneGraph->GetRoot());

  std::cout << "[SceneRenderer] RefreshMaterialTextures: Complete" << std::endl;
}

void SceneRenderer::CreateMaterialDescriptorSetsRecursive(GraphNode *node) {
  if (!node)
    return;

  for (const auto &mesh : node->GetMeshes()) {
    if (mesh) {
      auto material = mesh->GetMaterial();
      if (material && !material->HasDescriptorSet()) {
        // Ensure material has default textures for missing slots
        material->CheckRequiredTextures(m_Device);
        // Create the per-material descriptor set (with UBO + textures +
        // shadow map)
        material->CreateDescriptorSet(
            m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_DefaultTexture,
            m_UniformBuffers[0]->GetBuffer(), sizeof(UniformBufferObject),
            m_ShadowMap ? m_ShadowMap->GetCubeImageView() : VK_NULL_HANDLE,
            m_ShadowMap ? m_ShadowMap->GetSampler() : VK_NULL_HANDLE);
      }
    }
  }

  for (const auto &child : node->GetChildren()) {
    CreateMaterialDescriptorSetsRecursive(child.get());
  }
}

void SceneRenderer::InitializeShadowResources() {
  std::cout << "[SceneRenderer] InitializeShadowResources() called"
            << std::endl;

  // Create shadow map (1024x1024 cube map)
  m_ShadowMap = std::make_unique<PointShadowMap>();

  // Initialize PointShadowMap
  m_ShadowMap->Initialize(m_Device);

  // Initialize ShadowPipeline
  m_ShadowPipeline = std::make_unique<ShadowPipeline>(
      m_Device, "engine/shaders/ShadowDepth.vert.spv",
      "engine/shaders/ShadowDepth.frag.spv", m_ShadowMap->GetRenderPass());

  // Create debug Texture2D wrappers for each face
  m_FaceTextures.clear();
  for (uint32_t i = 0; i < PointShadowMap::NUM_FACES; i++) {
    auto faceView = m_ShadowMap->GetFaceImageView(i);
    // Use the shadow map's sampler (or create a new one if needed, but safe
    // to reuse)
    auto sampler = m_ShadowMap->GetSampler();
    int size = (int)m_ShadowMap->GetResolution();

    auto texture = std::make_unique<Vivid::Texture2D>(m_Device, faceView,
                                                      sampler, size, size);
    m_FaceTextures.push_back(std::move(texture));
  }
  std::cout << "[SceneRenderer] Shadow pipeline created" << std::endl;
}

void SceneRenderer::RenderShadowDebug(Vivid::Draw2D *draw2d) {
  if (!draw2d || m_FaceTextures.empty())
    return;

  // Draw 6 faces in a row at the top
  float size = 200.0f;
  float padding = 10.0f;
  float startX = 10.0f;
  float startY = 10.0f;

  for (size_t i = 0; i < m_FaceTextures.size(); i++) {
    glm::vec2 pos(startX + i * (size + padding), startY);
    glm::vec2 dim(size, size);

    // Draw colored border to identify faces
    glm::vec4 borderColor(1, 1, 1, 1);
    switch (i) {
    case 0:
      borderColor = glm::vec4(1, 0, 0, 1);
      break; // Red (+X)
    case 1:
      borderColor = glm::vec4(0, 1, 0, 1);
      break; // Green (-X)
    case 2:
      borderColor = glm::vec4(0, 0, 1, 1);
      break; // Blue (+Y)
    case 3:
      borderColor = glm::vec4(1, 1, 0, 1);
      break; // Yellow (-Y)
    case 4:
      borderColor = glm::vec4(0, 1, 1, 1);
      break; // Cyan (+Z)
    case 5:
      borderColor = glm::vec4(1, 0, 1, 1);
      break; // Magenta (-Z)
    }

    // Draw background rect (black) to see if texture is empty/transparent
    draw2d->DrawRectOutline(pos, dim, m_DefaultTexture.get(), borderColor);

    // Draw texture
    // Use red color to tint? No, white.
    // Ensure BlendMode handles single channel?
    // Usually single channel R is rendered as Red? Or Grayscale?
    // If Draw2D shader samples just RGB, and texture provides RRR1 or R001.
    draw2d->DrawTexture(pos, dim, m_FaceTextures[i].get(), glm::vec4(1.0f));
  }
}
void SceneRenderer::RenderShadowPass(VkCommandBuffer cmd) {
  if (!m_ShadowsEnabled || !m_ShadowMap || !m_ShadowPipeline || !m_SceneGraph) {
    return;
  }

  auto root = m_SceneGraph->GetRoot();
  if (!root) {
    return;
  }
  auto lp = m_SceneGraph->GetLightPosition();

  // m_MainLight->SetLocalPosition(lp);

  // Get light position from first light node or use default
  glm::vec3 lightPos = m_SceneGraph->GetLightPosition();
  lightPos = lp;

  // Update shadow map far plane to match light range (if applicable)
  float lightRange = 0.0f;
  if (!m_SceneGraph->GetLights().empty()) {
    lightRange = m_SceneGraph->GetLights()[0]->GetRange();
  }

  // If range is 0 (infinite), keep default 100.0f, otherwise sync
  if (lightRange > 0.0f) {
    m_ShadowMap->SetFarPlane(lightRange);
  }

  float farPlane = m_ShadowMap->GetFarPlane();

  // Render to each of the 6 cube faces
  for (uint32_t face = 0; face < 6; ++face) {
    glm::mat4 lightSpaceMatrix =
        m_ShadowMap->GetLightSpaceMatrix(lightPos, face);

    // Begin render pass for this face
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_ShadowMap->GetRenderPass();
    renderPassInfo.framebuffer = m_ShadowMap->GetFramebuffer(face);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent.width = m_ShadowMap->GetResolution();
    renderPassInfo.renderArea.extent.height = m_ShadowMap->GetResolution();

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_ShadowMap->GetResolution());
    viewport.height = static_cast<float>(m_ShadowMap->GetResolution());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = renderPassInfo.renderArea.extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind shadow pipeline
    m_ShadowPipeline->Bind(cmd);

    // Render scene to shadow map
    RenderNodeToShadow(cmd, root, lightSpaceMatrix);

    vkCmdEndRenderPass(cmd);
  }
}

void SceneRenderer::RenderNodeToShadow(VkCommandBuffer cmd, GraphNode *node,
                                       const glm::mat4 &lightSpaceMatrix) {
  if (!node) {
    return;
  }

  // Get light position and far plane
  glm::vec3 lightPos = m_SceneGraph->GetLightPosition();
  float farPlane = m_ShadowMap->GetFarPlane();

  auto l1 = m_SceneGraph->GetLights()[0];

  farPlane = l1->GetRange();

  node->SetLocalScale(1, 1, 1);

  // Render each mesh
  for (const auto &mesh : node->GetMeshes()) {
    if (mesh && mesh->IsFinalized()) {
      // Calculate model matrix
      glm::mat4 modelMatrix = node->GetWorldMatrix();

      // Push constants for shadow rendering
      ShadowPushConstants pc{};
      pc.lightSpaceMatrix = lightSpaceMatrix;
      pc.model = modelMatrix;
      pc.lightPos = glm::vec4(lightPos, farPlane); // Packed into w

      // Log matrices for the first face of the monkey to debug
      vkCmdPushConstants(cmd, m_ShadowPipeline->GetPipelineLayout(),
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(ShadowPushConstants), &pc);

      // Bind and draw mesh
      // std::cout << "Rendering shadow mesh for node: " << node->GetName() <<
      // " vertices: " << mesh->GetVertexCount() << std::endl;
      mesh->Bind(cmd);
      uint32_t indexCount = static_cast<uint32_t>(mesh->GetIndexCount());
      if (indexCount > 0) {
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
      }
    }
  }

  // Recursively render children
  for (const auto &child : node->GetChildren()) {
    RenderNodeToShadow(cmd, child.get(), lightSpaceMatrix);
  }
}

} // namespace Quantum
