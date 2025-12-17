#include "SceneRenderer.h"
#include "CameraNode.h"
#include "Draw2D.h"
#include "GraphNode.h"
#include "LightNode.h"
#include "Material.h"
#include "Mesh3D.h"
#include "RenderingPipelines.h"
#include "RotateGizmo.h"
#include "Texture2D.h"
#include "TranslateGizmo.h"
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

  std::cout << "[SceneRenderer] Creating descriptor pool..." << std::endl;
  CreateDescriptorPool();
  std::cout << "[SceneRenderer] Descriptor pool created successfully"
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
  std::vector<VkDescriptorSetLayout> layouts = {m_GlobalSetLayout,
                                                m_MaterialSetLayout};
  RenderingPipelines::Get().Initialize(m_Device, m_Renderer->GetRenderPass(),
                                       layouts);
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

  // Register Gizmo Pipeline (Push Constants, No UBO)
  Vivid::BlendConfig gizmoConfig;
  gizmoConfig.blendEnable = VK_FALSE;
  gizmoConfig.depthTestEnable = VK_FALSE;   // ALWAYS render on top of scene
  gizmoConfig.depthWriteEnable = VK_FALSE;  // Don't write to depth buffer
  gizmoConfig.cullMode = VK_CULL_MODE_NONE; // Disable culling for visibility
  gizmoConfig.pushConstantSize = 80;        // mat4 (64) + vec4 (16) = 80 bytes

  RenderingPipelines::Get().RegisterPipeline(
      "PLGizmoUnlit", "engine/shaders/PLGizmoUnlit.vert.spv",
      "engine/shaders/PLGizmoUnlit.frag.spv", gizmoConfig,
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

  // Initialize Gizmos (all types)
  std::cout << "[SceneRenderer] Initializing gizmos..." << std::endl;
  m_TranslateGizmo = std::make_unique<TranslateGizmo>(m_Device);
  m_RotateGizmo = std::make_unique<RotateGizmo>(m_Device);

  // Default to Translate gizmo
  m_ActiveGizmo = m_TranslateGizmo.get();

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

  // Depth testing for additive pass - use LESS_OR_EQUAL for proper
  // occlusion
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

  std::cout << "[SceneRenderer] PLPBR_Additive pipeline registered for "
               "multi-light"
            << std::endl;

  // Register debugging wireframe pipeline
  RegisterWireframePipeline();
  // Create Unit Cube for debugging
  m_UnitCube = Mesh3D::CreateUnitCube();
  if (m_UnitCube) {
    m_UnitCube->Finalize(m_Device);
  }

  // Initialize shadow mapping resources
  InitializeShadowResources();
  // Create descriptor sets (Global sets depend on shadow maps and UBOs)
  CreateDescriptorSets();

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
  for (auto &shadowMap : m_ShadowMaps) {
    if (shadowMap) {
      shadowMap->Shutdown();
    }
  }
  m_ShadowMaps.clear();

  std::cout << "[SceneRenderer] Resetting uniform buffers..." << std::endl;
  for (auto &buffer : m_UniformBuffers) {
    buffer.reset();
  }

  if (m_DescriptorPool != VK_NULL_HANDLE && m_Device) {
    std::cout << "[SceneRenderer] Destroying descriptor pool..." << std::endl;
    vkDestroyDescriptorPool(m_Device->GetDevice(), m_DescriptorPool, nullptr);
    m_DescriptorPool = VK_NULL_HANDLE;
  }

  if (m_GlobalSetLayout != VK_NULL_HANDLE && m_Device) {
    std::cout << "[SceneRenderer] Destroying global descriptor set layout..."
              << std::endl;
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_GlobalSetLayout,
                                 nullptr);
    m_GlobalSetLayout = VK_NULL_HANDLE;
  }

  if (m_MaterialSetLayout != VK_NULL_HANDLE && m_Device) {
    std::cout << "[SceneRenderer] Destroying material descriptor set layout..."
              << std::endl;
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_MaterialSetLayout,
                                 nullptr);
    m_MaterialSetLayout = VK_NULL_HANDLE;
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

  // Re-initialize resources that depend on the scene graph
  if (m_Initialized && m_SceneGraph) {
    InitializeShadowResources();
    CreateDescriptorSets();
    RefreshMaterialTextures();
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
  std::cout << "[SceneRenderer] Creating Split Descriptor Set Layouts..."
            << std::endl;

  // --- SET 0: GLOBAL (UBO + Shadow Map) ---
  VkDescriptorSetLayoutBinding uboBinding{};
  uboBinding.binding = 0;
  uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  uboBinding.descriptorCount = 1;
  uboBinding.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  uboBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding shadowBinding{};
  shadowBinding.binding = 1;
  shadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  shadowBinding.descriptorCount = 1;
  shadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  shadowBinding.pImmutableSamplers = nullptr;

  std::array<VkDescriptorSetLayoutBinding, 2> globalBindings = {uboBinding,
                                                                shadowBinding};

  VkDescriptorSetLayoutCreateInfo globalInfo{};
  globalInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  globalInfo.bindingCount = static_cast<uint32_t>(globalBindings.size());
  globalInfo.pBindings = globalBindings.data();

  if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &globalInfo, nullptr,
                                  &m_GlobalSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Global Descriptor Set Layout!");
  }

  // --- SET 1: MATERIAL (Albedo, Normal, Metallic, Roughness) ---
  VkDescriptorSetLayoutBinding materialBindings[4];
  for (int i = 0; i < 4; i++) {
    materialBindings[i].binding = i; // 0, 1, 2, 3
    materialBindings[i].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialBindings[i].descriptorCount = 1;
    materialBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    materialBindings[i].pImmutableSamplers = nullptr;
  }

  VkDescriptorSetLayoutCreateInfo materialInfo{};
  materialInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  materialInfo.bindingCount = 4;
  materialInfo.pBindings = materialBindings;

  if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &materialInfo, nullptr,
                                  &m_MaterialSetLayout) != VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create Material Descriptor Set Layout!");
  }

  std::cout << "[SceneRenderer] Descriptor Layouts Created (Global & Material)"
            << std::endl;
}

void SceneRenderer::CreateDescriptorPool() {
  std::cout << "[SceneRenderer] CreateDescriptorPool() started" << std::endl;

  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  poolSizes[0].descriptorCount = 200; // Enough for Global sets (Light*Frame)
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount =
      1000; // 400 Material textures + 100 Shadow Maps

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 1000; // Enough for Materials + Global Sets

  if (vkCreateDescriptorPool(m_Device->GetDevice(), &poolInfo, nullptr,
                             &m_DescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor pool!");
  }
}

void SceneRenderer::CreateDescriptorSets() {
  // Create Default Material Descriptor Set (for meshes with no material)
  if (m_DefaultMaterialSet == VK_NULL_HANDLE) {
    VkDescriptorSetAllocateInfo matAllocInfo{};
    matAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    matAllocInfo.descriptorPool = m_DescriptorPool;
    matAllocInfo.descriptorSetCount = 1;
    matAllocInfo.pSetLayouts = &m_MaterialSetLayout;

    if (vkAllocateDescriptorSets(m_Device->GetDevice(), &matAllocInfo,
                                 &m_DefaultMaterialSet) != VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate Default Material Set!");
    }

    std::array<VkDescriptorImageInfo, 4> defaultInfos;
    for (int i = 0; i < 4; i++) {
      defaultInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      defaultInfos[i].imageView = m_DefaultTexture->GetImageView();
      defaultInfos[i].sampler = m_DefaultTexture->GetSampler();
    }

    std::vector<VkWriteDescriptorSet> matWrites;
    for (int i = 0; i < 4; i++) {
      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_DefaultMaterialSet;
      write.dstBinding = i; // 0, 1, 2, 3
      write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.descriptorCount = 1;
      write.pImageInfo = &defaultInfos[i];
      matWrites.push_back(write);
    }
    vkUpdateDescriptorSets(m_Device->GetDevice(),
                           static_cast<uint32_t>(matWrites.size()),
                           matWrites.data(), 0, nullptr);
  }

  size_t numLights = m_ShadowMaps.size();
  size_t totalGlobalSets = numLights * MAX_FRAMES_IN_FLIGHT;

  if (totalGlobalSets == 0) {
    std::cout << "[SceneRenderer] No lights/shadows, valid but no global sets "
                 "created."
              << std::endl;
    return;
  }

  // Clear old sets if any (handled by pool reset usually, but here we just
  // overwrite vector) Actually we should free them if we are re-creating? Since
  // we don't reset the pool explicitly here, we rely on the vector being
  // cleared or resized. But vkAllocateDescriptorSets appends. Ideally we should
  // free old sets if this checks run multiple times? Current logic assumes
  // one-time creation or pool reset externally. For now, let's just proceed.

  m_GlobalDescriptorSets.resize(totalGlobalSets);

  std::vector<VkDescriptorSetLayout> layouts(totalGlobalSets,
                                             m_GlobalSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_DescriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(totalGlobalSets);
  allocInfo.pSetLayouts = layouts.data();

  if (vkAllocateDescriptorSets(m_Device->GetDevice(), &allocInfo,
                               m_GlobalDescriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate Global Descriptor Sets!");
  }

  // Bind resources for each set
  for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
    for (size_t light = 0; light < numLights; light++) {
      size_t setIndex = frame * numLights + light;
      VkDescriptorSet set = m_GlobalDescriptorSets[setIndex];

      // Binding 0: UBO (Use buffer for this frame)
      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = m_UniformBuffers[frame]->GetBuffer();
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet uboWrite{};
      uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      uboWrite.dstSet = set;
      uboWrite.dstBinding = 0;
      uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      uboWrite.descriptorCount = 1;
      uboWrite.pBufferInfo = &bufferInfo;

      // Binding 1: Shadow Map
      VkDescriptorImageInfo shadowInfo{};
      shadowInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      shadowInfo.imageView = m_ShadowMaps[light]->GetCubeImageView();
      shadowInfo.sampler = m_ShadowMaps[light]->GetSampler();

      VkWriteDescriptorSet shadowWrite{};
      shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      shadowWrite.dstSet = set;
      shadowWrite.dstBinding = 1;
      shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      shadowWrite.descriptorCount = 1;
      shadowWrite.pImageInfo = &shadowInfo;

      std::array<VkWriteDescriptorSet, 2> writes = {uboWrite, shadowWrite};
      vkUpdateDescriptorSets(m_Device->GetDevice(),
                             static_cast<uint32_t>(writes.size()),
                             writes.data(), 0, nullptr);
    }
  }

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

  // Create dedicated Gizmo UBO buffer (small, holds only a few gizmo draws)
  size_t gizmoMaxDraws = 16; // Max 16 gizmo parts (XYZ axes = 3)
  VkDeviceSize gizmoBufferSize = m_AlignedUBOSize * gizmoMaxDraws;
  m_GizmoUniformBuffer = std::make_unique<Vivid::VividBuffer>(
      m_Device, gizmoBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_GizmoUniformBuffer->Map();
  std::cout << "[SceneRenderer] Created dedicated Gizmo UBO buffer ("
            << gizmoBufferSize << " bytes)" << std::endl;

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

  // Update all global descriptor sets with new buffer handles
  if (m_ShadowMaps.size() > 0) {
    for (size_t i = 0; i < m_GlobalDescriptorSets.size(); ++i) {
      size_t frameIndex = i / m_ShadowMaps.size();
      if (frameIndex >= MAX_FRAMES_IN_FLIGHT)
        continue;

      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = m_UniformBuffers[frameIndex]->GetBuffer();
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet descriptorWrite{};
      descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrite.dstSet = m_GlobalDescriptorSets[i];
      descriptorWrite.dstBinding = 0;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType =
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pBufferInfo = &bufferInfo;

      vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &descriptorWrite, 0,
                             nullptr);
    }
  }

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
  m_CurrentDrawIndex = 0;      // Reset draw index for the new frame
  m_CurrentPipeline = nullptr; // Reset pipeline tracking to force rebind
  m_CurrentTexture = nullptr;  // Reset texture tracking
  m_GizmoDrawIndex = 0;        // Reset gizmo draw index

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
      // Linear accumulation: m_CurrentDrawIndex continues growing across
      // light passes This packs the UBO tightly: [Light0_Draws]
      // [Light1_Draws] ...

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

  // Render Gizmos (Overlay) after scene - only if there's a selected node
  if (m_ActiveGizmo && m_ActiveGizmo->GetTargetNode()) {
    auto camera = m_SceneGraph->GetCurrentCamera();
    if (camera) {
      glm::mat4 view = camera->GetWorldMatrix(); // CameraNode::GetWorldMatrix
                                                 // returns View Matrix
      float aspect = (float)width / (float)height;
      glm::mat4 proj =
          glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
      proj[1][1] *= -1; // Vulkan clip space Y flip

      m_ActiveGizmo->Render(this, cmd, view, proj);
    }
  }
}

void SceneRenderer::DrawGizmoMesh(VkCommandBuffer cmd,
                                  std::shared_ptr<Mesh3D> mesh,
                                  const glm::mat4 &model,
                                  const glm::vec3 &color, const glm::mat4 &view,
                                  const glm::mat4 &proj) {
  if (!mesh || !mesh->IsFinalized())
    return;

  // Use PLGizmoUnlit pipeline (push constants, no UBO)
  auto pipeline = RenderingPipelines::Get().GetPipeline("PLGizmoUnlit");
  if (!pipeline) {
    std::cerr << "[SceneRenderer] PLGizmoUnlit pipeline not found!"
              << std::endl;
    return;
  }

  pipeline->Bind(cmd);

  // Push constants struct (must match shader layout)
  struct GizmoPushConstants {
    glm::mat4 mvp;
    glm::vec4 color;
  };

  GizmoPushConstants pc;
  pc.mvp = proj * view * model;
  pc.color = glm::vec4(color, 1.0f);

  // Push constants - no UBO, no descriptor sets needed for transform/color!
  vkCmdPushConstants(cmd, pipeline->GetPipelineLayout(),
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GizmoPushConstants),
                     &pc);

  // Draw mesh (no descriptor sets needed for this simple unlit shader)
  mesh->Bind(cmd);
  vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh->GetIndexCount()), 1, 0, 0,
                   0);
}

void SceneRenderer::SetGizmoPosition(const glm::vec3 &position) {
  if (m_ActiveGizmo) {
    m_ActiveGizmo->SetPosition(position);
  }
}

void SceneRenderer::SetGizmoTargetNode(std::shared_ptr<GraphNode> node) {
  if (m_ActiveGizmo) {
    m_ActiveGizmo->SetTargetNode(node);
  }
}

void SceneRenderer::SetGizmoViewState(const glm::mat4 &view,
                                      const glm::mat4 &proj, int w, int h) {
  if (m_ActiveGizmo) {
    m_ActiveGizmo->SetViewState(view, proj, w, h);
  }
}

bool SceneRenderer::OnGizmoMouseClicked(int x, int y, bool isPressed, int width,
                                        int height) {
  if (m_ActiveGizmo) {
    return m_ActiveGizmo->OnMouseClicked(x, y, isPressed, width, height);
  }
  return false;
}

void SceneRenderer::OnGizmoMouseMoved(int x, int y) {
  if (m_ActiveGizmo) {
    m_ActiveGizmo->OnMouseMoved(x, y);
  }
}

bool SceneRenderer::IsGizmoDragging() const {
  if (m_ActiveGizmo) {
    return m_ActiveGizmo->IsDragging();
  }
  return false;
}

void SceneRenderer::SetGizmoSpace(GizmoSpace space) {
  if (m_ActiveGizmo) {
    m_ActiveGizmo->SetSpace(space);
  }
}

void SceneRenderer::SetGizmoType(GizmoType type) {
  GizmoBase *newGizmo = nullptr;

  switch (type) {
  case GizmoType::Translate:
    newGizmo = m_TranslateGizmo.get();
    break;
  case GizmoType::Rotate:
    newGizmo = m_RotateGizmo.get();
    break;
  case GizmoType::Scale:
    // Not implemented yet, fall back to translate
    newGizmo = m_TranslateGizmo.get();
    break;
  }

  if (newGizmo && newGizmo != m_ActiveGizmo) {
    // Transfer state from old gizmo to new gizmo
    if (m_ActiveGizmo) {
      newGizmo->SetPosition(m_ActiveGizmo->GetPosition());
      newGizmo->SetTargetNode(m_ActiveGizmo->GetTargetNode());
      newGizmo->SetSpace(m_ActiveGizmo->GetSpace());
      // ViewState is set each frame, no need to transfer
    }
    m_ActiveGizmo = newGizmo;
    std::cout << "[SceneRenderer] Switched to gizmo type: "
              << static_cast<int>(type) << std::endl;
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

    // Use current light based on m_CurrentLightIndex (set by RenderScene
    // loop)
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
    // std::cout << "Light Pos: " << ubo.lightPos.x << "," << ubo.lightPos.y
    // <<
    // "," << ubo.lightPos.z << std::endl;

    ubo.padding2 = 0.0f;

    // Debug: Log light data for each pass (temporary)

    // DYNAMIC BUFFER STRATEGY:
    // We strictly use linear indexing. Check capacity.
    if (m_CurrentDrawIndex >= m_MaxDraws) {
      // Buffer full, cannot write debug info or draw
      // (This check should ideally be in the mesh loop for safety, but we do
      // it here for log consistency)
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

          // Bind the GLOBAL descriptor set (Set 0)
          // Includes UBO (dynamic offset) and Shadow Map for this light/frame
          size_t globalSetIndex =
              m_CurrentFrameIndex * m_ShadowMaps.size() + m_CurrentLightIndex;
          if (globalSetIndex < m_GlobalDescriptorSets.size()) {
            VkDescriptorSet globalSet = m_GlobalDescriptorSets[globalSetIndex];
            uint32_t dynamicOffset =
                static_cast<uint32_t>(m_CurrentDrawIndex * m_AlignedUBOSize);

            // Upload UBO data
            void *mappedData =
                m_UniformBuffers[m_CurrentFrameIndex]->GetMappedMemory();
            if (mappedData) {
              char *dest = (char *)mappedData + dynamicOffset;
              memcpy(dest, &ubo, sizeof(UniformBufferObject));
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    meshPipeline->GetPipelineLayout(), 0, 1,
                                    &globalSet, 1, &dynamicOffset);
          } else {
            // Fallback or error?
            // Should not happen if sets allocated correctly
          }

          // Bind the MATERIAL descriptor set (Set 1)
          VkDescriptorSet materialSet = m_DefaultMaterialSet;
          if (material && material->HasDescriptorSet()) {
            materialSet = material->GetDescriptorSet();
          }
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  meshPipeline->GetPipelineLayout(), 1, 1,
                                  &materialSet, 0, nullptr);

          mesh->Bind(cmd);
          uint32_t indexCount = static_cast<uint32_t>(mesh->GetIndexCount());
          if (indexCount > 0) {
            vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
            m_RenderMeshCount++;
          }

          m_CurrentDrawIndex++;
        }
      }
    }
  }

  // Recursively render children
  for (const auto &child : node->GetChildren()) {
    RenderNode(cmd, child.get(), width, height);
  }
}

void SceneRenderer::UpdateTextureDescriptor(Vivid::Texture2D *texture) {
  // Obsolete - functionality moved to Material/Global sets
}

void SceneRenderer::UpdatePBRTextures(Material *material) {
  // Obsolete - functionality moved to Material/Global sets
}

void SceneRenderer::UpdateFirstMaterialTextures(GraphNode *node) {
  // Obsolete
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
        // Create Material Set (Set 1): Textures only
        material->CreateDescriptorSet(m_Device, m_DescriptorPool,
                                      m_MaterialSetLayout, m_DefaultTexture);
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

  if (!m_SceneGraph) {
    std::cout
        << "[SceneRenderer] No scene graph, skipping shadow initialization"
        << std::endl;
    return;
  }

  // Clear existing shadow maps
  m_ShadowMaps.clear();
  m_FaceTextures.clear(); // We'll disable debug textures for now or just
                          // show first

  // Get lights from scene
  auto lights = m_SceneGraph->GetLights();
  int pointLightCount = 0;
  for (const auto &light : lights) {
    if (light->GetType() == LightNode::LightType::Point) {
      pointLightCount++;
      auto shadowMap = std::make_unique<PointShadowMap>();
      shadowMap->Initialize(m_Device);
      m_ShadowMaps.push_back(std::move(shadowMap));
    }
  }

  std::cout << "[SceneRenderer] Created " << m_ShadowMaps.size()
            << " shadow maps for " << pointLightCount << " point lights"
            << std::endl;

  if (m_ShadowMaps.empty()) {
    std::cout << "[SceneRenderer] No point lights, skipping pipeline creation"
              << std::endl;
    return;
  }

  // Initialize ShadowPipeline (Reuse render pass from first shadow map)
  // Assuming all shadow maps have compatible render passes (they should)
  m_ShadowPipeline = std::make_unique<ShadowPipeline>(
      m_Device, "engine/shaders/ShadowDepth.vert.spv",
      "engine/shaders/ShadowDepth.frag.spv", m_ShadowMaps[0]->GetRenderPass());

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
  if (!m_ShadowsEnabled || !m_ShadowPipeline || !m_SceneGraph) {
    return;
  }

  auto lights = m_SceneGraph->GetLights();
  if (lights.empty())
    return;
  auto root = m_SceneGraph->GetRoot();
  if (!root)
    return;

  // Render to each shadow map
  for (size_t i = 0; i < m_ShadowMaps.size(); i++) {
    // Skip if light index is invalid
    if (i >= lights.size())
      break;

    auto light = lights[i];
    auto shadowMap = m_ShadowMaps[i].get();
    glm::vec3 lightPos = light->GetWorldPosition();
    float farPlane = light->GetRange();
    if (farPlane <= 0.0f)
      farPlane = 100.0f; // Default

    shadowMap->SetFarPlane(farPlane);

    // Render 6 faces
    for (uint32_t face = 0; face < 6; ++face) {
      glm::mat4 lightSpaceMatrix =
          shadowMap->GetLightSpaceMatrix(lightPos, face);

      VkRenderPassBeginInfo renderPassInfo{};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass = shadowMap->GetRenderPass();
      renderPassInfo.framebuffer = shadowMap->GetFramebuffer(face);
      renderPassInfo.renderArea.offset = {0, 0};
      renderPassInfo.renderArea.extent.width = shadowMap->GetResolution();
      renderPassInfo.renderArea.extent.height = shadowMap->GetResolution();

      VkClearValue clearValue{};
      clearValue.depthStencil = {1.0f, 0};
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues = &clearValue;

      vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport{};
      viewport.width = static_cast<float>(shadowMap->GetResolution());
      viewport.height = static_cast<float>(shadowMap->GetResolution());
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(cmd, 0, 1, &viewport);

      VkRect2D scissor{};
      scissor.extent = renderPassInfo.renderArea.extent;
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      m_ShadowPipeline->Bind(cmd);
      RenderNodeToShadow(cmd, root, lightSpaceMatrix, lightPos, farPlane);

      vkCmdEndRenderPass(cmd);
    }
  }
}

void SceneRenderer::RenderNodeToShadow(VkCommandBuffer cmd, GraphNode *node,
                                       const glm::mat4 &lightSpaceMatrix,
                                       const glm::vec3 &lightPos,
                                       float farPlane) {
  if (!node)
    return;

  // Render each mesh
  for (const auto &mesh : node->GetMeshes()) {
    if (mesh && mesh->IsFinalized()) {
      ShadowPushConstants pc{};
      pc.lightSpaceMatrix = lightSpaceMatrix;
      pc.model = node->GetWorldMatrix();
      pc.lightPos = glm::vec4(lightPos, farPlane);

      vkCmdPushConstants(cmd, m_ShadowPipeline->GetPipelineLayout(),
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(ShadowPushConstants), &pc);

      mesh->Bind(cmd);
      uint32_t indexCount = static_cast<uint32_t>(mesh->GetIndexCount());
      if (indexCount > 0) {
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
      }
    }
  }

  for (const auto &child : node->GetChildren()) {
    RenderNodeToShadow(cmd, child.get(), lightSpaceMatrix, lightPos, farPlane);
  }
}

void SceneRenderer::RegisterWireframePipeline() {
  std::cout << "[SceneRenderer] Registering Wireframe Pipeline..." << std::endl;
  Vivid::BlendConfig wireframeConfig;
  wireframeConfig.blendEnable = VK_FALSE;
  wireframeConfig.depthTestEnable = VK_TRUE;
  wireframeConfig.depthWriteEnable = VK_TRUE;
  wireframeConfig.depthCompareOp =
      VK_COMPARE_OP_LESS_OR_EQUAL; // Allow drawing on top of geometry
  wireframeConfig.polygonMode = VK_POLYGON_MODE_LINE; // Wireframe
  wireframeConfig.lineWidth = 2.0f;                   // Thicker lines
  wireframeConfig.depthBiasEnable = VK_TRUE;
  wireframeConfig.depthBiasConstantFactor = -2.0f; // Bias towards camera
  wireframeConfig.depthBiasSlopeFactor = -2.0f;

  // Re-use PLSimple shaders for wireframe (solid color)
  RenderingPipelines::Get().RegisterPipeline(
      "PLSimple_Wireframe", "engine/shaders/PLSimple.vert.spv",
      "engine/shaders/PLSimple.frag.spv", wireframeConfig,
      Vivid::PipelineType::Mesh3D);
}

void SceneRenderer::RenderSelection(VkCommandBuffer cmd,
                                    std::shared_ptr<GraphNode> selectedNode) {
  if (!selectedNode || !m_UnitCube)
    return;

  // Wireframe pipeline
  auto *pipeline = RenderingPipelines::Get().GetPipeline("PLSimple_Wireframe");
  if (!pipeline)
    return;

  pipeline->Bind(cmd);

  // Calculate World Bounds
  glm::vec3 min, max;
  selectedNode->GetWorldBounds(min, max);

  // If bounds are invalid (degenerate), don't draw
  if (min == max)
    return;

  // Calculate Transform Data
  glm::vec3 center = (min + max) * 0.5f;
  glm::vec3 size = max - min;
  glm::vec3 scale = size; // Unit cube is 1x1x1, so scale is just size

  // Construct Transform Matrix for the Unit Cube
  // Selection box should be axis-aligned to world, so rotation is identity
  glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
  //  model = glm::scale(model, scale);

  // Update UBO?
  // We need to push this model matrix to the shader.
  // PLSimple uses the standard UBO layout (Set 0).
  // We can reuse the current frame's UBO buffer but we need to write to it.
  // CRITICAL: We need a dynamic offset or push constants for the model matrix
  // to do this efficiently without flushing.
  //
  // For now, since SceneRenderer::RenderNode writes per-draw using dynamic
  // offsets, we should do the same. But we are outside the main loop.
  // AND we need a slot in the UBO buffer.
  //
  // Hack for now: Use a reserved slot or just check if we have space.
  // Current RenderScene logic increments m_CurrentDrawIndex. We can use that!

  if (m_CurrentDrawIndex >= m_MaxDraws) {
    return; // No space in UBO
  }

  // --- Update UBO for Selection Box ---
  UniformBufferObject ubo{};
  ubo.model = model;

  // Use same Camera/Proj as last used (stored where?)
  // We need to access the camera again.
  auto camera = m_SceneGraph->GetCurrentCamera();
  if (camera) {
    ubo.view = camera->GetWorldMatrix();
    ubo.viewPos = camera->GetWorldPosition();
  } else {
    // Fallback
    ubo.view = glm::mat4(1.0f);
  }
  // Projection logic duplicates RenderNode... should ideally refactor getProj
  int width = Vivid::VividApplication::GetFrameWidth();
  int height = Vivid::VividApplication::GetFrameHeight();
  if (width <= 0)
    width = 800;
  if (height <= 0)
    height = 600;

  ubo.proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height,
                              0.1f, 100.0f);
  ubo.proj[1][1] *= -1;

  // Yellow Color for selection
  ubo.lightColor = glm::vec3(
      1.0f, 1.0f, 0.0f); // Re-purpose lightColor as object color for PLSimple
  ubo.lightPos = glm::vec3(0.0f); // Unused by simple shader usually, or ignored
  ubo.lightRange = 0.0f;

  // Write to Buffer
  // Note: This relies on m_UniformBuffers being mapped.
  VkDeviceSize drawOffset = m_CurrentDrawIndex * m_AlignedUBOSize;
  // Assuming m_CurrentFrameIndex is 0 as per RenderScene TODO
  size_t frameIndex = 0; // m_CurrentFrameIndex;

  m_UniformBuffers[frameIndex]->WriteToBuffer(&ubo, sizeof(UniformBufferObject),
                                              drawOffset);

  // Bind Global Descriptor (Dynamic Offset)
  // Reuse binding logic
  size_t globalSetIndex =
      frameIndex *
      m_SceneGraph->GetLights().size(); // Use 0th light slot equivalent
  if (globalSetIndex < m_GlobalDescriptorSets.size()) {
    VkDescriptorSet globalSet = m_GlobalDescriptorSets[globalSetIndex];

    uint32_t dynamicOffset = static_cast<uint32_t>(drawOffset);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->GetPipelineLayout(), 0, 1, &globalSet, 1,
                            &dynamicOffset);
  }

  // Default Material Set (White)
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->GetPipelineLayout(), 1, 1,
                          &m_DefaultMaterialSet, 0, nullptr);

  // Draw Cube
  m_UnitCube->Bind(cmd);
  vkCmdDrawIndexed(cmd, static_cast<uint32_t>(m_UnitCube->GetIndexCount()), 1,
                   0, 0, 0);

  m_CurrentDrawIndex++;
}

} // namespace Quantum
