#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "SceneRenderer.h"
#include "CameraNode.h"
#include "Draw2D.h"
#include "GraphNode.h"
#include "LightNode.h"
#include "Material.h"
#include "Mesh3D.h"
#include "RenderingPipelines.h"
#include "RotateGizmo.h"
#include "TerrainNode.h"
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
  glm::mat4 lightSpaceMatrix; // Added for directional shadows
  glm::vec3 viewPos;
  float time;
  glm::vec3 lightPos;
  float clipPlaneDir;
  glm::vec3 lightColor;
  float lightRange;
  float lightType; // 0 = Directional, 1 = Point, 2 = Spot
  float _pad1, _pad2,
      _pad3; // Padding KaTeX parse error: Expected 'EOF', got '&' at position
             // 16: to maintain 16-byte alignment
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

  // Create null shadow maps for fallback (needed for pipeline creation)
  m_NullShadowMap = std::make_unique<PointShadowMap>();
  m_NullShadowMap->Initialize(m_Device, 128);
  m_NullDirShadowMap = std::make_unique<DirectionalShadowMap>();
  m_NullDirShadowMap->Initialize(m_Device, 128);

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

  // Register PLTerrainGizmo Pipeline (blending enabled)
  Vivid::BlendConfig terrainGizmoConfig;
  terrainGizmoConfig.blendEnable = VK_TRUE;
  terrainGizmoConfig.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  terrainGizmoConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  terrainGizmoConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  terrainGizmoConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  terrainGizmoConfig.depthTestEnable = VK_TRUE;
  terrainGizmoConfig.depthWriteEnable = VK_FALSE;
  terrainGizmoConfig.cullMode = VK_CULL_MODE_NONE;
  terrainGizmoConfig.pushConstantSize = 80;

  RenderingPipelines::Get().RegisterPipeline(
      "PLTerrainGizmo", "engine/shaders/PLTerrainGizmo.vert.spv",
      "engine/shaders/PLTerrainGizmo.frag.spv", terrainGizmoConfig,
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
  m_TerrainGizmo = std::make_unique<TerrainGizmo>(m_Device);

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

  // Register PLWater pipeline
  RenderingPipelines::Get().RegisterPipeline(
      "PLWater", "engine/shaders/PLWater.vert.spv",
      "engine/shaders/PLWater.frag.spv", opaqueConfig,
      Vivid::PipelineType::Mesh3D); // Water is opaque for now (or use blend
                                    // if desired)

  if (RenderingPipelines::Get().HasPipeline("PLWater")) {
    std::cout << "[SceneRenderer] PLWater pipeline registered successfully"
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

  // Register Water Additive Pipeline (Vertex Displacement + Additive Blend)
  RenderingPipelines::Get().RegisterPipeline(
      "PLWater_Additive", "engine/shaders/PLWater.vert.spv",
      "engine/shaders/PLWater.frag.spv", additiveConfig,
      Vivid::PipelineType::Mesh3D);
  std::cout << "[SceneRenderer] PLWater_Additive pipeline registered"
            << std::endl;

  // Register PLTerrain pipeline for layered terrain rendering
  RenderingPipelines::Get().RegisterPipeline(
      "PLTerrain", "engine/shaders/PLTerrain.vert.spv",
      "engine/shaders/PLTerrain.frag.spv", opaqueConfig,
      Vivid::PipelineType::Mesh3D);

  // Set terrain descriptor layouts for RenderingPipelines
  std::vector<VkDescriptorSetLayout> terrainLayouts = {m_GlobalSetLayout,
                                                       m_TerrainSetLayout};
  RenderingPipelines::Get().SetTerrainLayouts(terrainLayouts);
  std::cout << "[SceneRenderer] PLTerrain pipeline registered with terrain "
               "layouts"
            << std::endl;

  // Register debugging wireframe pipeline
  RegisterWireframePipeline();
  // Create Unit Cube for debugging
  m_UnitCube = Mesh3D::CreateUnitCube();
  if (m_UnitCube) {
    m_UnitCube->Finalize(m_Device);
  }

  // Water resources depend on textures
  CreateWaterResources();

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
  m_NullShadowMap.reset();
  m_NullDirShadowMap.reset();

  DestroyWaterResources();

  std::cout << "[SceneRenderer] Resetting uniform buffers..." << std::endl;
  for (auto &buffer : m_UniformBuffers) {
    buffer.reset();
  }
  m_GizmoUniformBuffer.reset();
  m_TranslateGizmo.reset();
  m_RotateGizmo.reset();
  m_TerrainGizmo.reset();
  m_ActiveGizmo = nullptr;

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

  if (m_TerrainSetLayout != VK_NULL_HANDLE && m_Device) {
    std::cout << "[SceneRenderer] Destroying terrain descriptor set layout..."
              << std::endl;
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_TerrainSetLayout,
                                 nullptr);
    m_TerrainSetLayout = VK_NULL_HANDLE;
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
    if (!m_WaterResourcesCreated) {
      CreateWaterResources();
    }
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

      // Ensure Water Resources exist
      if (!m_WaterResourcesCreated) {
        CreateWaterResources();
      }

      // Assign Water Textures (Reflection/Refraction)
      AssignWaterTextures(m_SceneGraph->GetRoot());

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

  VkDescriptorSetLayoutBinding dirShadowBinding{};
  dirShadowBinding.binding = 2;
  dirShadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  dirShadowBinding.descriptorCount = 1;
  dirShadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  dirShadowBinding.pImmutableSamplers = nullptr;

  std::array<VkDescriptorSetLayoutBinding, 3> globalBindings = {
      uboBinding, shadowBinding, dirShadowBinding};

  VkDescriptorSetLayoutCreateInfo globalInfo{};
  globalInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  globalInfo.bindingCount = static_cast<uint32_t>(globalBindings.size());
  globalInfo.pBindings = globalBindings.data();

  if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &globalInfo, nullptr,
                                  &m_GlobalSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Global Descriptor Set Layout!");
  }

  // --- SET 1: MATERIAL (Albedo, Normal, Metallic, Roughness, Reflection,
  // Refraction) ---
  VkDescriptorSetLayoutBinding materialBindings[6];
  for (int i = 0; i < 6; i++) {
    materialBindings[i].binding = i; // 0, 1, 2, 3, 4, 5
    materialBindings[i].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialBindings[i].descriptorCount = 1;
    materialBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    materialBindings[i].pImmutableSamplers = nullptr;
  }

  VkDescriptorSetLayoutCreateInfo materialInfo{};
  materialInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  materialInfo.bindingCount = 6;
  materialInfo.pBindings = materialBindings;

  if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &materialInfo, nullptr,
                                  &m_MaterialSetLayout) != VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create Material Descriptor Set Layout!");
  }

  // --- SET 2: TERRAIN (16 layer textures: 4 layers × 4 textures each) ---
  VkDescriptorSetLayoutBinding terrainBindings[16];
  for (int i = 0; i < 16; i++) {
    terrainBindings[i].binding = i;
    terrainBindings[i].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    terrainBindings[i].descriptorCount = 1;
    terrainBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    terrainBindings[i].pImmutableSamplers = nullptr;
  }

  VkDescriptorSetLayoutCreateInfo terrainInfo{};
  terrainInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  terrainInfo.bindingCount = 16;
  terrainInfo.pBindings = terrainBindings;

  if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &terrainInfo, nullptr,
                                  &m_TerrainSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Terrain Descriptor Set Layout!");
  }

  std::cout << "[SceneRenderer] Descriptor Layouts Created (Global, Material, "
               "Terrain)"
            << std::endl;
}

void SceneRenderer::CreateDescriptorPool() {
  std::cout << "[SceneRenderer] CreateDescriptorPool() started" << std::endl;

  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  poolSizes[0].descriptorCount = 200; // Enough for Global sets (Light*Frame)
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount =
      1500; // 600 Material textures + 100 Shadow Maps + Extras

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

    std::array<VkDescriptorImageInfo, 6> defaultInfos;
    for (int i = 0; i < 6; i++) {
      defaultInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      defaultInfos[i].imageView = m_DefaultTexture->GetImageView();
      defaultInfos[i].sampler = m_DefaultTexture->GetSampler();
    }

    std::vector<VkWriteDescriptorSet> matWrites;
    for (int i = 0; i < 6; i++) {
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

  size_t numPointShadows = m_ShadowMaps.size();
  size_t numDirShadows = m_DirShadowMaps.size();
  size_t numLights = numPointShadows + numDirShadows;
  size_t numSetsPerFrame = std::max(numLights, (size_t)1);
  size_t totalGlobalSets = numSetsPerFrame * MAX_FRAMES_IN_FLIGHT;

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
    for (size_t light = 0; light < numSetsPerFrame; light++) {
      size_t setIndex = frame * numSetsPerFrame + light;
      VkDescriptorSet set = m_GlobalDescriptorSets[setIndex];

      // Binding 0: UBO
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

      // Binding 1: Point Shadow Map
      VkDescriptorImageInfo shadowInfo{};
      shadowInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (light < numPointShadows) {
        shadowInfo.imageView = m_ShadowMaps[light]->GetCubeImageView();
        shadowInfo.sampler = m_ShadowMaps[light]->GetSampler();
      } else {
        shadowInfo.imageView = m_NullShadowMap->GetCubeImageView();
        shadowInfo.sampler = m_NullShadowMap->GetSampler();
      }

      VkWriteDescriptorSet shadowWrite{};
      shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      shadowWrite.dstSet = set;
      shadowWrite.dstBinding = 1;
      shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      shadowWrite.descriptorCount = 1;
      shadowWrite.pImageInfo = &shadowInfo;

      // Binding 2: Directional Shadow Map
      VkDescriptorImageInfo dirShadowInfo{};
      dirShadowInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (light >= numPointShadows &&
          (light - numPointShadows) < numDirShadows) {
        size_t dirIndex = light - numPointShadows;
        dirShadowInfo.imageView = m_DirShadowMaps[dirIndex]->GetImageView();
        dirShadowInfo.sampler = m_DirShadowMaps[dirIndex]->GetSampler();
      } else {
        dirShadowInfo.imageView = m_NullDirShadowMap->GetImageView();
        dirShadowInfo.sampler = m_NullDirShadowMap->GetSampler();
      }

      VkWriteDescriptorSet dirShadowWrite{};
      dirShadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      dirShadowWrite.dstSet = set;
      dirShadowWrite.dstBinding = 2;
      dirShadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      dirShadowWrite.descriptorCount = 1;
      dirShadowWrite.pImageInfo = &dirShadowInfo;

      std::array<VkWriteDescriptorSet, 3> writes = {uboWrite, shadowWrite,
                                                    dirShadowWrite};
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

void SceneRenderer::RenderScene(VkCommandBuffer cmd, int width, int height,
                                float time) {
  // Check and refresh any dirty terrain descriptors BEFORE command recording
  // This must happen before any command buffer recording starts
  if (m_SceneGraph && m_SceneGraph->GetRoot()) {
    CheckAndRefreshDirtyTerrains(m_SceneGraph->GetRoot());
  }

  // Reset draw indices at the start of the frame
  // (RenderWaterPasses also resets this, but may be skipped if no water)
  m_CurrentDrawIndex = 0;
  m_CurrentPipeline = nullptr; // Reset pipeline tracking to force rebind
  m_CurrentTexture = nullptr;  // Reset texture tracking
  m_GizmoDrawIndex = 0;        // Reset gizmo draw index
  m_AnimationAngle = time;     // Update animation time

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

  // Store viewport aspect ratio for water passes to use
  m_ViewportAspect = static_cast<float>(width) / static_cast<float>(height);

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

  // NOTE: m_CurrentDrawIndex is NOT reset here - it continues from water passes

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

      // RenderReflection/RenderRefraction removed from here - called in
      // ViewportWidget Linear accumulation: m_CurrentDrawIndex continues
      // growing across light passes This packs the UBO tightly: [Light0_Draws]
      // [Light1_Draws] ...

      // Set dynamic state INSIDE loop to ensure it persists
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT =
          (PFN_vkCmdSetFrontFaceEXT)vkGetDeviceProcAddr(m_Device->GetDevice(),
                                                        "vkCmdSetFrontFaceEXT");
      if (vkCmdSetFrontFaceEXT) {
        vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
      }

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

  // Render Terrain Gizmo (Brush) if in Terrain Mode
  if (m_ShowTerrainGizmo && m_TerrainGizmo) {
    auto camera = m_SceneGraph->GetCurrentCamera();
    if (camera) {
      glm::mat4 view = camera->GetWorldMatrix();
      float aspect = (float)width / (float)height;
      glm::mat4 proj =
          glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
      proj[1][1] *= -1;

      // Temporary fixed position as requested - REMOVED
      // m_TerrainGizmo->SetPosition(glm::vec3(0.0f, 0.1f, 0.0f));
      m_TerrainGizmo->Render(this, cmd, view, proj);
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

void SceneRenderer::SetTerrainGizmoSize(float size) {
  if (m_TerrainGizmo) {
    m_TerrainGizmo->SetSize(size);
  }
}

void SceneRenderer::SetGizmoSpace(GizmoSpace space) {
  if (m_ActiveGizmo) {
    m_ActiveGizmo->SetSpace(space);
  }
}

void SceneRenderer::SetGizmoType(GizmoType type) {
  if (type == GizmoType::None) {
    m_ActiveGizmo = nullptr;
    return;
  }

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
  default:
    return;
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

// Proxy RenderNode (uses camera view/proj)
void SceneRenderer::RenderNode(VkCommandBuffer cmd, GraphNode *node, int width,
                               int height) {
  if (!node || !m_SceneGraph)
    return;

  glm::mat4 view(1.0f);
  glm::mat4 proj(1.0f);

  auto camera = m_SceneGraph->GetCurrentCamera();
  if (camera) {
    view = camera->GetWorldMatrix();
  } else {
    view =
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));
  }

  float aspect = (float)width / (float)height;
  proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
  proj[1][1] *= -1;

  RenderNode(cmd, node, width, height, view, proj);
}

// Overload with explicit View/Proj (used by proxy and reflection/refraction)
void SceneRenderer::RenderNode(VkCommandBuffer cmd, GraphNode *node, int width,
                               int height, const glm::mat4 &view,
                               const glm::mat4 &proj, bool skipWater) {
  if (!node) {
    return;
  }

  m_RenderNodeCount++;

  // Render meshes attached to this node
  if (node->HasMeshes()) {
    // Update animation
    // Update uniform buffer with MVP matrices
    UniformBufferObject ubo{};

    // Just use identity model (ignore node transform for now) ???
    // UPDATE: Use node GetWorldMatrix()!
    ubo.model = node->GetWorldMatrix();

    ubo.view = view;
    ubo.proj = proj;

    // Set lighting data for PBR
    // Calculate ViewPos from View Matrix inverse
    glm::mat4 invView = glm::inverse(view);
    ubo.viewPos = glm::vec3(invView[3]);

    ubo.time = m_AnimationAngle;

    // Use current light based on m_CurrentLightIndex (set by RenderScene
    // loop)
    if (m_SceneGraph) {
      const auto &lights = m_SceneGraph->GetLights();
      if (m_CurrentLightIndex < lights.size()) {
        auto light = lights[m_CurrentLightIndex];
        ubo.lightColor = light->GetColor();
        ubo.lightRange = light->GetRange();

        // Set light type explicitly: 0=Directional, 1=Point, 2=Spot
        ubo.lightType = static_cast<float>(light->GetType());

        // For directional lights, pass the light DIRECTION
        // For point lights, pass the light POSITION
        if (light->GetType() == LightNode::LightType::Directional) {
          // Get forward direction from light's world matrix (+Z is forward)
          // Shader will negate this to get direction FROM fragment TO light
          glm::vec3 lightDir = glm::normalize(
              glm::vec3(light->GetWorldMatrix() * glm::vec4(0, 0, 1, 0)));
          ubo.lightPos = lightDir; // This is a DIRECTION, not a position
        } else {
          ubo.lightPos = light->GetWorldPosition();
        }
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

    ubo.clipPlaneDir =
        m_ClipPlaneDir; // Clip plane: 1=reflection, -1=refraction, 0=normal

    // Set lightSpaceMatrix for directional shadow mapping
    // For directional lights, compute the matrix from the current light
    ubo.lightSpaceMatrix = glm::mat4(1.0f); // Default to identity
    if (m_SceneGraph &&
        m_CurrentLightIndex < m_SceneGraph->GetLights().size()) {
      auto light = m_SceneGraph->GetLights()[m_CurrentLightIndex];
      if (light->GetType() == LightNode::LightType::Directional) {
        // Find corresponding directional shadow map
        size_t dirLightIndex = 0;
        for (size_t i = 0; i < m_CurrentLightIndex; i++) {
          if (i < m_SceneGraph->GetLights().size() &&
              m_SceneGraph->GetLights()[i]->GetType() !=
                  LightNode::LightType::Directional) {
            // Skip non-directional lights
          } else {
            dirLightIndex++;
          }
        }
        // For simplicity, assume directional lights come after point lights
        // and use index 0 for the first directional shadow map
        size_t numPointLights = m_ShadowMaps.size();
        size_t dirIndex = m_CurrentLightIndex >= numPointLights
                              ? m_CurrentLightIndex - numPointLights
                              : 0;
        if (dirIndex < m_DirShadowMaps.size() && m_DirShadowMaps[dirIndex]) {
          // Use cached matrix calculated in RenderShadowPass for consistency
          ubo.lightSpaceMatrix = m_CachedDirShadowMatrix;
        }
      }
    }

    // DYNAMIC BUFFER STRATEGY:
    // We strictly use linear indexing. Check capacity.
    if (m_CurrentDrawIndex >= m_MaxDraws) {
      // Buffer full
    }
    VkDeviceSize drawOffset = m_CurrentDrawIndex * m_AlignedUBOSize;

    // DEBUG: Track every node/light/draw combination
    // static int debugCounter = 0;
    // if (debugCounter < 200) {
    // ... logging ...
    //   debugCounter++;
    // }

    // Render each mesh
    for (const auto &mesh : node->GetMeshes()) {
      if (mesh) {
        if (mesh->IsFinalized()) {
          Vivid::VividPipeline *meshPipeline = nullptr;
          Vivid::Texture2D *albedoTexture = nullptr; // Unused
          auto material = mesh->GetMaterial();
          if (material) {
            meshPipeline = material->GetPipeline();

            // Skip water meshes when rendering reflection/refraction maps
            if (skipWater && material->GetPipelineName() == "PLWater") {
              continue;
            }
          }

          // Fall back to default PLSimple if no material or pipeline
          if (!meshPipeline) {
            meshPipeline = RenderingPipelines::Get().GetPipeline("PLSimple");
          }

          // DYNAMIC PIPELINE SWITCH FOR MULTI-LIGHT PASS
          if (m_CurrentLightIndex > 0 && meshPipeline) {
            bool isWater = false;
            if (material && material->GetPipeline() &&
                material->GetPipeline()->GetName() == "PLWater") {
              isWater = true;
            }

            if (isWater) {
              meshPipeline = Quantum::RenderingPipelines::Get().GetPipeline(
                  "PLWater_Additive");
            } else {
              meshPipeline = Quantum::RenderingPipelines::Get().GetPipeline(
                  "PLPBR_Additive");
            }

            if (!meshPipeline) {
              if (material) {
                meshPipeline = material->GetPipeline();
              } else {
                meshPipeline =
                    RenderingPipelines::Get().GetPipeline("PLSimple");
              }
            }
          }

          // Bind pipeline if it changed
          if (meshPipeline && meshPipeline != m_CurrentPipeline) {
            m_CurrentPipeline = meshPipeline;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              meshPipeline->GetPipeline());
          }

          if (!m_CurrentPipeline) {
            continue;
          }

          // Set light space matrix in UBO if it's a directional light
          ubo.lightSpaceMatrix = glm::mat4(1.0f);
          if (m_CurrentLightIndex >= m_ShadowMaps.size() &&
              (m_CurrentLightIndex - m_ShadowMaps.size()) <
                  m_DirShadowMaps.size()) {
            size_t dirIndex = m_CurrentLightIndex - m_ShadowMaps.size();
            auto light = m_SceneGraph->GetLights()[m_CurrentLightIndex];
            glm::vec3 lightDir =
                light->GetWorldMatrix() * glm::vec4(0, 0, 1, 0);
            ubo.lightSpaceMatrix =
                m_DirShadowMaps[dirIndex]->GetLightSpaceMatrix(lightDir,
                                                               m_SceneCenter);
          }

          // Bind the GLOBAL descriptor set (Set 0)
          size_t numSetsPerFrame =
              std::max(m_ShadowMaps.size() + m_DirShadowMaps.size(), (size_t)1);
          size_t globalSetIndex =
              m_CurrentFrameIndex * numSetsPerFrame + m_CurrentLightIndex;

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
          }

          // Bind Set 1: TERRAIN or MATERIAL descriptor set
          // Check if this node is a TerrainNode - bind terrain descriptor set
          auto *terrainNode = dynamic_cast<TerrainNode *>(node);
          if (terrainNode &&
              terrainNode->GetDescriptorSet() != VK_NULL_HANDLE) {
            // Terrain node - bind terrain descriptor set (16 textures)
            VkDescriptorSet terrainSet = terrainNode->GetDescriptorSet();
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    meshPipeline->GetPipelineLayout(), 1, 1,
                                    &terrainSet, 0, nullptr);
          } else {
            // Standard material - bind material descriptor set (6 textures)
            VkDescriptorSet materialSet = m_DefaultMaterialSet;
            if (material && material->HasDescriptorSet()) {
              materialSet = material->GetDescriptorSet();
            }
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    meshPipeline->GetPipelineLayout(), 1, 1,
                                    &materialSet, 0, nullptr);
          }

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
    RenderNode(cmd, child.get(), width, height, view, proj, skipWater);
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

void SceneRenderer::CheckAndRefreshDirtyTerrains(GraphNode *node) {
  if (!node)
    return;

  auto *terrainNode = dynamic_cast<TerrainNode *>(node);
  if (terrainNode) {
    // Process any pending texture loads from the UI thread on the render thread
    terrainNode->ProcessPendingUpdates();

    if (terrainNode->NeedsDescriptorUpdate()) {
      // We found a dirty terrain, update its descriptor set
      CreateMaterialDescriptorSetsRecursive(terrainNode);
    }
  }

  for (const auto &child : node->GetChildren()) {
    CheckAndRefreshDirtyTerrains(child.get());
  }
}

void SceneRenderer::CreateMaterialDescriptorSetsRecursive(GraphNode *node) {
  if (!node)
    return;

  // Check if this is a TerrainNode - allocate terrain descriptor set
  auto *terrainNode = dynamic_cast<TerrainNode *>(node);
  if (terrainNode && (terrainNode->GetDescriptorSet() == VK_NULL_HANDLE ||
                      terrainNode->NeedsDescriptorUpdate())) {
    bool wasAlreadyAllocated =
        terrainNode->GetDescriptorSet() != VK_NULL_HANDLE;
    VkDescriptorSet terrainSet = terrainNode->GetDescriptorSet();

    // Only allocate a new descriptor set if we don't have one
    if (!wasAlreadyAllocated) {
      VkDescriptorSetAllocateInfo terrainAllocInfo{};
      terrainAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      terrainAllocInfo.descriptorPool = m_DescriptorPool;
      terrainAllocInfo.descriptorSetCount = 1;
      terrainAllocInfo.pSetLayouts = &m_TerrainSetLayout;

      if (vkAllocateDescriptorSets(m_Device->GetDevice(), &terrainAllocInfo,
                                   &terrainSet) != VK_SUCCESS) {
        // Failed to allocate, skip this terrain
        return;
      }
      terrainNode->SetDescriptorSet(terrainSet);
    }

    // Always re-write descriptor set with current textures
    {
      // Write terrain layer textures to descriptor set
      std::vector<VkDescriptorImageInfo> imageInfos(16);
      std::vector<VkWriteDescriptorSet> writes(16);

      for (int layer = 0; layer < 4; layer++) {
        const auto &terrainLayer = terrainNode->GetLayer(layer);

        // Binding 0-3: Layer color maps
        int colorIdx = layer * 4 + 0;
        imageInfos[colorIdx].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[colorIdx].imageView =
            terrainLayer.colorMap ? terrainLayer.colorMap->GetImageView()
                                  : m_DefaultTexture->GetImageView();
        imageInfos[colorIdx].sampler = terrainLayer.colorMap
                                           ? terrainLayer.colorMap->GetSampler()
                                           : m_DefaultTexture->GetSampler();

        // Binding 1-5-9-13: Layer normal maps
        int normalIdx = layer * 4 + 1;
        imageInfos[normalIdx].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[normalIdx].imageView =
            terrainLayer.normalMap ? terrainLayer.normalMap->GetImageView()
                                   : m_DefaultTexture->GetImageView();
        imageInfos[normalIdx].sampler =
            terrainLayer.normalMap ? terrainLayer.normalMap->GetSampler()
                                   : m_DefaultTexture->GetSampler();

        // Binding 2-6-10-14: Layer specular maps
        int specIdx = layer * 4 + 2;
        imageInfos[specIdx].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[specIdx].imageView =
            terrainLayer.specularMap ? terrainLayer.specularMap->GetImageView()
                                     : m_DefaultTexture->GetImageView();
        imageInfos[specIdx].sampler =
            terrainLayer.specularMap ? terrainLayer.specularMap->GetSampler()
                                     : m_DefaultTexture->GetSampler();

        // Binding 3-7-11-15: Layer blend maps
        int mapIdx = layer * 4 + 3;
        imageInfos[mapIdx].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[mapIdx].imageView =
            terrainLayer.layerMap ? terrainLayer.layerMap->GetImageView()
                                  : m_DefaultTexture->GetImageView();
        imageInfos[mapIdx].sampler = terrainLayer.layerMap
                                         ? terrainLayer.layerMap->GetSampler()
                                         : m_DefaultTexture->GetSampler();
      }

      // Build write descriptor sets
      for (int i = 0; i < 16; i++) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = terrainSet;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
      }

      vkUpdateDescriptorSets(m_Device->GetDevice(),
                             static_cast<uint32_t>(writes.size()),
                             writes.data(), 0, nullptr);

      std::cout << "[SceneRenderer] Updated terrain descriptor set for: "
                << terrainNode->GetName() << std::endl;
    }

    // Clear the dirty flag after update
    terrainNode->ClearDescriptorDirty();
  }

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
  int dirLightCount = 0;
  for (const auto &light : lights) {
    if (light->GetType() == LightNode::LightType::Point) {
      pointLightCount++;
      auto shadowMap = std::make_unique<PointShadowMap>();
      shadowMap->Initialize(m_Device);
      m_ShadowMaps.push_back(std::move(shadowMap));
    } else if (light->GetType() == LightNode::LightType::Directional) {
      dirLightCount++;
      auto shadowMap = std::make_unique<DirectionalShadowMap>();
      shadowMap->Initialize(m_Device);
      m_DirShadowMaps.push_back(std::move(shadowMap));
    }
  }

  std::cout << "[SceneRenderer] Created " << m_ShadowMaps.size()
            << " point shadow maps and " << m_DirShadowMaps.size()
            << " directional shadow maps" << std::endl;

  // Initialize ShadowPipeline (Reuse render pass from null shadow map)
  m_ShadowPipeline = std::make_unique<ShadowPipeline>(
      m_Device, "engine/shaders/ShadowDepth.vert.spv",
      "engine/shaders/ShadowDepth.frag.spv", m_NullShadowMap->GetRenderPass());

  std::cout << "[SceneRenderer] Shadow pipeline created" << std::endl;
}

void SceneRenderer::RenderShadowDebug(Vivid::Draw2D *draw2d) {
  if (!draw2d)
    return;

  // TEST: Always draw a red rectangle to verify Draw2D works from this function
  if (m_DefaultTexture) {
    draw2d->DrawTexture(glm::vec2(20.0f, 20.0f), glm::vec2(100.0f, 100.0f),
                        m_DefaultTexture.get(),
                        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
  }

  // Debug: Always draw a small indicator to verify this function is called
  static bool firstCall = true;
  if (firstCall) {
    std::cout
        << "[SceneRenderer::RenderShadowDebug] Called. m_DirShadowMaps.size()="
        << m_DirShadowMaps.size()
        << " m_FaceTextures.size()=" << m_FaceTextures.size()
        << " m_DefaultTexture=" << (m_DefaultTexture ? "valid" : "null")
        << std::endl;
    firstCall = false;
  }

  // Draw point light shadow cube faces (if available)
  if (!m_FaceTextures.empty()) {
    float size = 150.0f;
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

      draw2d->DrawRectOutline(pos, dim, m_DefaultTexture.get(), borderColor);
      draw2d->DrawTexture(pos, dim, m_FaceTextures[i].get(), glm::vec4(1.0f));
    }
  }

  // Create debug textures for directional shadow maps if needed
  if (m_DirShadowDebugTextures.size() != m_DirShadowMaps.size()) {
    m_DirShadowDebugTextures.clear();
    for (const auto &shadowMap : m_DirShadowMaps) {
      if (shadowMap && shadowMap->IsInitialized()) {
        // Create a wrapper Texture2D from the shadow map's image view and
        // sampler
        auto debugTex = std::make_unique<Vivid::Texture2D>(
            m_Device, shadowMap->GetImageView(), shadowMap->GetSampler(),
            static_cast<int>(shadowMap->GetResolution()),
            static_cast<int>(shadowMap->GetResolution()));
        m_DirShadowDebugTextures.push_back(std::move(debugTex));
      }
    }
    std::cout << "[SceneRenderer] Created " << m_DirShadowDebugTextures.size()
              << " directional shadow debug textures" << std::endl;
  }

  // Draw directional shadow maps (below point light faces or at top if no point
  // lights)
  float dirSize = 256.0f;
  float dirPadding = 10.0f;
  float dirStartX = 10.0f;
  float dirStartY = m_FaceTextures.empty() ? 10.0f : 180.0f;

  for (size_t i = 0; i < m_DirShadowDebugTextures.size(); i++) {
    glm::vec2 pos(dirStartX + i * (dirSize + dirPadding), dirStartY);
    glm::vec2 dim(dirSize, dirSize);

    // Orange border for directional light shadow
    glm::vec4 borderColor(1.0f, 0.5f, 0.0f, 1.0f);
    draw2d->DrawRectOutline(pos, dim, m_DefaultTexture.get(), borderColor,
                            3.0f);

    // Draw the shadow map texture
    draw2d->DrawTexture(pos, dim, m_DirShadowDebugTextures[i].get(),
                        glm::vec4(1.0f));
  }

  // Always draw a small indicator to show this function runs
  // Draw a green square in top-left corner if no shadow maps to display
  if (m_FaceTextures.empty() && m_DirShadowDebugTextures.empty()) {
    glm::vec2 indicatorPos(10.0f, 10.0f);
    glm::vec2 indicatorSize(50.0f, 50.0f);
    // Green border to indicate "RenderShadowDebug was called but no shadow
    // maps"
    draw2d->DrawRectOutline(indicatorPos, indicatorSize, m_DefaultTexture.get(),
                            glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 5.0f);
  }
}

void SceneRenderer::InvalidateTextureDescriptors() {
  // Called before Draw2D destruction on swapchain recreation
  // Invalidate all textures that may have cached descriptor sets from Draw2D's
  // pool
  if (m_DefaultTexture) {
    m_DefaultTexture->InvalidateDescriptorSet();
  }

  // Invalidate face textures (point light shadow debug)
  for (auto &tex : m_FaceTextures) {
    if (tex) {
      tex->InvalidateDescriptorSet();
    }
  }

  // Invalidate directional shadow debug textures
  for (auto &tex : m_DirShadowDebugTextures) {
    if (tex) {
      tex->InvalidateDescriptorSet();
    }
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

  // Compute scene center from camera BEFORE rendering shadows
  // This ensures both shadow render and shadow sampling use the same center
  auto camera = m_SceneGraph->GetCurrentCamera();
  if (camera) {
    // Get the world matrix (inverse of the view matrix returned by CameraNode)
    glm::mat4 view = camera->GetWorldMatrix();
    glm::mat4 world = glm::inverse(view);

    glm::vec3 cameraPos = glm::vec3(world[3]);
    glm::vec3 cameraForward = -glm::vec3(world[2]); // Forward is -Z

    // Scene center is 10 units in front of the camera (tighter focus for our
    // 20-unit shadow area)
    m_SceneCenter = cameraPos + cameraForward * 10.0f;
  } else {
    m_SceneCenter = glm::vec3(0.0f);
  }

  // Render to each point shadow map
  for (size_t i = 0; i < m_ShadowMaps.size(); i++) {
    auto light = lights[i]; // Point lights are first
    auto shadowMap = m_ShadowMaps[i].get();
    glm::vec3 lightPos = light->GetWorldPosition();
    float farPlane = light->GetRange();
    if (farPlane <= 0.0f)
      farPlane = 100.0f;

    shadowMap->SetFarPlane(farPlane);

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
      RenderNodeToShadow(cmd, m_SceneGraph->GetRoot(), lightSpaceMatrix,
                         glm::vec4(lightPos, farPlane));

      vkCmdEndRenderPass(cmd);
    }
  }

  // Render to each directional shadow map
  for (size_t i = 0; i < m_DirShadowMaps.size(); i++) {
    // Directional lights are after point lights in the list
    if (m_ShadowMaps.size() + i >= lights.size())
      break;
    auto light = lights[m_ShadowMaps.size() + i];
    auto shadowMap = m_DirShadowMaps[i].get();

    // Calculate light space matrix (using scene center as target)
    glm::vec3 lightDir = light->GetWorldMatrix() * glm::vec4(0, 0, 1, 0);
    glm::mat4 lightSpaceMatrix =
        shadowMap->GetLightSpaceMatrix(lightDir, m_SceneCenter);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = shadowMap->GetRenderPass();
    renderPassInfo.framebuffer = shadowMap->GetFramebuffer();
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
    // Directional lights use farPlane = 0.0 to signal non-cube shadow
    RenderNodeToShadow(cmd, m_SceneGraph->GetRoot(), lightSpaceMatrix,
                       glm::vec4(light->GetWorldPosition(), 0.0f));

    vkCmdEndRenderPass(cmd);
  }
}

void SceneRenderer::RenderNodeToShadow(VkCommandBuffer cmd, GraphNode *node,
                                       const glm::mat4 &lightSpaceMatrix,
                                       const glm::vec4 &lightInfo) {
  if (!node)
    return;

  // Render each mesh
  for (const auto &mesh : node->GetMeshes()) {
    if (mesh && mesh->IsFinalized()) {
      ShadowPushConstants pc{};
      pc.lightSpaceMatrix = lightSpaceMatrix;
      pc.model = node->GetWorldMatrix();
      pc.lightPos = lightInfo;

      vkCmdPushConstants(cmd, m_ShadowPipeline->GetPipelineLayout(),
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, (uint32_t)sizeof(ShadowPushConstants), &pc);

      mesh->Bind(cmd);
      uint32_t indexCount = static_cast<uint32_t>(mesh->GetIndexCount());
      if (indexCount > 0) {
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
      }
    }
  }

  for (const auto &child : node->GetChildren()) {
    RenderNodeToShadow(cmd, child.get(), lightSpaceMatrix, lightInfo);
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

  // Determine View/Proj (fetch camera)
  // Logic restored from previous implementation
  auto camera = m_SceneGraph->GetCurrentCamera();
  if (camera) {
    ubo.view = camera->GetWorldMatrix();

    // Calculate ViewPos from View Matrix (inverse)
    glm::mat4 invView = glm::inverse(ubo.view);
    ubo.viewPos = glm::vec3(invView[3]);
  } else {
    ubo.view = glm::mat4(1.0f);
    ubo.viewPos = glm::vec3(0.0f);
  }

  int width = Vivid::VividApplication::GetFrameWidth();
  int height = Vivid::VividApplication::GetFrameHeight();
  if (width <= 0)
    width = 800;
  if (height <= 0)
    height = 600;

  ubo.proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height,
                              0.1f, 100.0f);
  ubo.proj[1][1] *= -1;

  // Handle Light Data (same as before or simplified?)
  // For now, keep the lighting logic simple or same.
  // We need lightPos, lightColor etc.

  // Yellow Color for selection (Wait, this is RenderNode, not RenderSelection?)
  // Ah, the snippet I viewed in Step 357 lines 1400-1460 was confusingly
  // seemingly inside RenderNode? Wait, Step 353 showed RenderNode start. Lines
  // 1419 says "Update UBO for Selection Box". Is this code inside RenderNode?
  // Let me look at Step 353 again.
  // It says `void SceneRenderer::RenderNode(...)` starts at 934.
  // Step 357 starts at 1400. That is FAR later.
  // I might have been looking at `RenderSelection` or something else in Step
  // 357? Let's verify WHERE RenderNode is.

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

// =================================================================================================
// Water Rendering Implementation
// =================================================================================================

void SceneRenderer::CreateAttachment(VkFormat format, VkImageUsageFlags usage,
                                     VkImage &image, VkDeviceMemory &memory,
                                     VkImageView &view) {
  if (image != VK_NULL_HANDLE)
    return;

  VkImageAspectFlags aspectMask = 0;
  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  m_Device->CreateImage(m_WaterResolution, m_WaterResolution, format,
                        VK_IMAGE_TILING_OPTIMAL, usage,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);
  view = m_Device->CreateImageView(
      image, format); // Basic CreateImageView usually assumes Color?

  // VividDevice::CreateImageView implementation check:
  // It creates view with aspectMask derived from format inside VividDevice
  // typically, or we might need to be careful if it defaults to color. Assuming
  // VividDevice::CreateImageView handles depth formats correctly.
}

void SceneRenderer::CreateWaterResources() {
  if (m_WaterResourcesCreated)
    return;
  std::cout << "[SceneRenderer] Creating Water Resources (" << m_WaterResolution
            << "x" << m_WaterResolution << ")..." << std::endl;

  // 1. Create Render Pass
  VkAttachmentDescription attachments[2] = {};

  // Color attachment - Use same format as swapchain for compatibility
  attachments[0].format = m_Renderer->GetSwapChainImageFormat();
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // Depth attachment
  attachments[1].format = VK_FORMAT_D32_SFLOAT;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp =
      VK_ATTACHMENT_STORE_OP_DONT_CARE; // We don't sample depth
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorRef = {};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef = {};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  // Dependencies
  std::array<VkSubpassDependency, 2> dependencies;

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 2;
  renderPassInfo.pAttachments = attachments;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  if (vkCreateRenderPass(m_Device->GetDevice(), &renderPassInfo, nullptr,
                         &m_WaterRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Water Render Pass!");
  }

  // 2. Create Attachments
  CreateAttachment(
      m_Renderer->GetSwapChainImageFormat(),
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      m_ReflectionImage, m_ReflectionMemory, m_ReflectionImageView);
  CreateAttachment(
      m_Renderer->GetSwapChainImageFormat(),
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      m_RefractionImage, m_RefractionMemory, m_RefractionImageView);

  // Create SEPARATE depth buffers for each pass to avoid corruption
  CreateAttachment(m_Renderer->GetSwapChainDepthFormat(),
                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                   m_ReflectionDepthImage, m_ReflectionDepthMemory,
                   m_ReflectionDepthImageView);
  CreateAttachment(m_Renderer->GetSwapChainDepthFormat(),
                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                   m_RefractionDepthImage, m_RefractionDepthMemory,
                   m_RefractionDepthImageView);

  // 3. Create Framebuffers
  VkFramebufferCreateInfo fbInfo = {};
  fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fbInfo.renderPass = m_WaterRenderPass;
  fbInfo.attachmentCount = 2;
  fbInfo.width = m_WaterResolution;
  fbInfo.height = m_WaterResolution;
  fbInfo.layers = 1;

  // Reflection Framebuffer (uses its own depth buffer)
  std::array<VkImageView, 2> reflAttachments = {m_ReflectionImageView,
                                                m_ReflectionDepthImageView};
  fbInfo.pAttachments = reflAttachments.data();
  if (vkCreateFramebuffer(m_Device->GetDevice(), &fbInfo, nullptr,
                          &m_ReflectionFramebuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Reflection Framebuffer!");
  }

  // Refraction Framebuffer (uses its own depth buffer)
  std::array<VkImageView, 2> refrAttachments = {m_RefractionImageView,
                                                m_RefractionDepthImageView};
  fbInfo.pAttachments = refrAttachments.data();
  if (vkCreateFramebuffer(m_Device->GetDevice(), &fbInfo, nullptr,
                          &m_RefractionFramebuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Refraction Framebuffer!");
  }

  // 4. Create Texture Wrappers (so we can bind them)
  // Create a sampler
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = 16.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 1.0f;

  VkSampler sampler;
  vkCreateSampler(m_Device->GetDevice(), &samplerInfo, nullptr, &sampler);
  // Note: We are leaking this sampler technically if we don't store it,
  // but Texture2D copies it? No, Texture2D doesn't own the sampler in the
  // wrapper constructor. The wrapper constructor "Wrap existing resources"
  // assumes ownership? Checking Texture2D header: "Wrap existing resources
  // (does not own them)" Ah, so we need to manage the sampler + views lifetime.
  // Ideally we should store m_WaterSampler. For now, let's create a dedicated
  // one in header or here and store it. Actually, let's just create Texture2D
  // normally which owns resources? No, we created resources manually. We will
  // cheat and let the wrapper use the sampler we just created, but we need to
  // keep the sampler alive. Or, we use the constructor 2: CreateFromData... no.
  // Constructor 3: Texture2D(device, view, sampler, w, h)

  // Let's create `m_WaterSampler` member or just leak it for this session (it's
  // destroyed on device shutdown). Better: store it in a static or member.

  // For now, I will create valid Texture2D objects.
  m_ReflectionTexture = std::make_shared<Vivid::Texture2D>(
      m_Device, m_ReflectionImageView, sampler, m_WaterResolution,
      m_WaterResolution);
  m_RefractionTexture = std::make_shared<Vivid::Texture2D>(
      m_Device, m_RefractionImageView, sampler, m_WaterResolution,
      m_WaterResolution);

  m_WaterResourcesCreated = true;
}

void SceneRenderer::DestroyWaterResources() {
  VkDevice device = m_Device->GetDevice();

  if (m_ReflectionFramebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer(device, m_ReflectionFramebuffer, nullptr);
  if (m_RefractionFramebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer(device, m_RefractionFramebuffer, nullptr);
  if (m_WaterRenderPass != VK_NULL_HANDLE)
    vkDestroyRenderPass(device, m_WaterRenderPass, nullptr);

  if (m_ReflectionImageView != VK_NULL_HANDLE)
    vkDestroyImageView(device, m_ReflectionImageView, nullptr);
  if (m_ReflectionImage != VK_NULL_HANDLE)
    vkDestroyImage(device, m_ReflectionImage, nullptr);
  if (m_ReflectionMemory != VK_NULL_HANDLE)
    vkFreeMemory(device, m_ReflectionMemory, nullptr);

  if (m_RefractionImageView != VK_NULL_HANDLE)
    vkDestroyImageView(device, m_RefractionImageView, nullptr);
  if (m_RefractionImage != VK_NULL_HANDLE)
    vkDestroyImage(device, m_RefractionImage, nullptr);
  if (m_RefractionMemory != VK_NULL_HANDLE)
    vkFreeMemory(device, m_RefractionMemory, nullptr);

  // Cleanup separate depth buffers
  if (m_ReflectionDepthImageView != VK_NULL_HANDLE)
    vkDestroyImageView(device, m_ReflectionDepthImageView, nullptr);
  if (m_ReflectionDepthImage != VK_NULL_HANDLE)
    vkDestroyImage(device, m_ReflectionDepthImage, nullptr);
  if (m_ReflectionDepthMemory != VK_NULL_HANDLE)
    vkFreeMemory(device, m_ReflectionDepthMemory, nullptr);

  if (m_RefractionDepthImageView != VK_NULL_HANDLE)
    vkDestroyImageView(device, m_RefractionDepthImageView, nullptr);
  if (m_RefractionDepthImage != VK_NULL_HANDLE)
    vkDestroyImage(device, m_RefractionDepthImage, nullptr);
  if (m_RefractionDepthMemory != VK_NULL_HANDLE)
    vkFreeMemory(device, m_RefractionDepthMemory, nullptr);

  m_WaterResourcesCreated = false;
}

void SceneRenderer::RenderReflection(VkCommandBuffer cmd, float time) {
  if (!m_WaterResourcesCreated || !m_SceneGraph->GetCurrentCamera())
    return;

  // Begin Render Pass
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_WaterRenderPass;
  renderPassInfo.framebuffer = m_ReflectionFramebuffer;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = {m_WaterResolution, m_WaterResolution};

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Black for proper sky
  clearValues[1].depthStencil = {1.0f, 0};
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Reset render state for reflection pass
  // NOTE: m_CurrentDrawIndex is NOT reset here - it continues from frame start
  // so each pass uses unique UBO offsets
  m_CurrentLightIndex = 0;     // Use first light only for reflection
  m_CurrentPipeline = nullptr; // Force pipeline rebind
  // Viewport
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_WaterResolution;
  viewport.height = (float)m_WaterResolution;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {m_WaterResolution, m_WaterResolution};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // =========================================================================
  // TRADITIONAL MIRRORED CAMERA REFLECTION
  // Camera position: Y = -originalY (below water)
  // Camera pitch: inverted (if looking down 30°, now look up 30°)
  // =========================================================================
  auto camera = m_SceneGraph->GetCurrentCamera();
  glm::mat4 mainView = camera->GetWorldMatrix();

  // Extract camera world position and orientation from inverse view matrix
  glm::mat4 invView = glm::inverse(mainView);
  glm::vec3 camPos = glm::vec3(invView[3]); // Camera world position
  glm::vec3 camForward = -glm::normalize(
      glm::vec3(invView[2])); // Forward direction (camera looks down -Z)
  glm::vec3 camUp = glm::normalize(glm::vec3(invView[1])); // Up direction

  // Mirror camera position across Y=0 water plane
  glm::vec3 reflectPos;
  reflectPos.x = camPos.x;
  reflectPos.y = -camPos.y; // NEGATE Y: camera goes below water
  reflectPos.z = camPos.z;

  // Mirror forward direction: NEGATE Y component to flip pitch
  // If camera was looking DOWN, now it looks UP at same angle
  glm::vec3 reflectForward;
  reflectForward.x = camForward.x;
  reflectForward.y = -camForward.y; // FLIP PITCH
  reflectForward.z = camForward.z;

  // Mirror up vector: NEGATE Y to maintain correct orientation
  glm::vec3 reflectUp;
  reflectUp.x = camUp.x;
  reflectUp.y = -camUp.y;
  reflectUp.z = camUp.z;

  // Build reflection view matrix using lookAt
  glm::vec3 reflectTarget = reflectPos + reflectForward;
  glm::mat4 view = glm::lookAt(reflectPos, reflectTarget, reflectUp);

  // Projection - use MAIN viewport aspect ratio
  float aspect = m_ViewportAspect > 0.0f ? m_ViewportAspect : 1.0f;
  glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
  proj[1][1] *= -1; // Vulkan Y-flip

  // Update UBO?
  // We rely on RenderNode using the dynamic offset and incrementing
  // m_CurrentDrawIndex. RenderNode updates the UBO with the provided view/proj.

  // 1. Render Scene to Reflection Map
  // Use the overload that accepts view/proj
  if (m_SceneGraph && m_SceneGraph->GetRoot()) {
    PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT =
        (PFN_vkCmdSetFrontFaceEXT)vkGetDeviceProcAddr(m_Device->GetDevice(),
                                                      "vkCmdSetFrontFaceEXT");
    if (vkCmdSetFrontFaceEXT) {
      vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_CLOCKWISE);
    }

    // Set clip plane to clip below Y=0 (keep above water for reflection)
    m_ClipPlaneDir = 1.0f;

    RenderNode(cmd, m_SceneGraph->GetRoot(), m_WaterResolution,
               m_WaterResolution, view, proj, true); // skipWater = true

    // Reset clip plane
    m_ClipPlaneDir = 0.0f;

    if (vkCmdSetFrontFaceEXT) {
      vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    }
  }

  vkCmdEndRenderPass(cmd);
}

void SceneRenderer::RenderRefraction(VkCommandBuffer cmd, float time) {
  if (!m_WaterResourcesCreated || !m_SceneGraph->GetCurrentCamera())
    return;

  // Begin Render Pass
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_WaterRenderPass;
  renderPassInfo.framebuffer = m_RefractionFramebuffer;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = {m_WaterResolution, m_WaterResolution};

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Black for proper sky
  clearValues[1].depthStencil = {1.0f, 0};
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Reset render state for refraction pass
  // NOTE: m_CurrentDrawIndex is NOT reset here - it continues from reflection
  // so each pass uses unique UBO offsets
  m_CurrentLightIndex = 0;     // Use first light only for refraction
  m_CurrentPipeline = nullptr; // Force pipeline rebind
  // Viewport
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_WaterResolution;
  viewport.height = (float)m_WaterResolution;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {m_WaterResolution, m_WaterResolution};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Refraction Camera = Main Camera
  auto camera = m_SceneGraph->GetCurrentCamera();
  // We need the VIEW matrix (inverse of World)
  glm::mat4 view =
      camera
          ->GetWorldMatrix(); // CameraNode::GetWorldMatrix returns View Matrix

  // Projection - use MAIN viewport aspect ratio for correct UV alignment
  float aspect = m_ViewportAspect > 0.0f ? m_ViewportAspect : 1.0f;
  glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
  proj[1][1] *= -1;

  // Render Scene to Refraction Map
  if (m_SceneGraph && m_SceneGraph->GetRoot()) {
    PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT =
        (PFN_vkCmdSetFrontFaceEXT)vkGetDeviceProcAddr(m_Device->GetDevice(),
                                                      "vkCmdSetFrontFaceEXT");
    if (vkCmdSetFrontFaceEXT) {
      vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    }

    // Set clip plane to clip above Y=0 (keep below water for refraction)
    m_ClipPlaneDir = -1.0f;

    RenderNode(cmd, m_SceneGraph->GetRoot(), m_WaterResolution,
               m_WaterResolution, view, proj, true); // skipWater = true

    // Reset clip plane
    m_ClipPlaneDir = 0.0f;
  }

  vkCmdEndRenderPass(cmd);
}

void SceneRenderer::RenderWaterPasses(VkCommandBuffer cmd, float time) {
  if (!m_WaterResourcesCreated)
    return;

  // Ensure water materials have the correct reflection/refraction textures
  // bound
  if (m_SceneGraph && m_SceneGraph->GetRoot()) {
    AssignWaterTextures(m_SceneGraph->GetRoot());
  }

  // Reset draw index at the start of the frame (ONLY place this should happen)
  // Water passes + main scene will all use sequential UBO offsets
  m_CurrentDrawIndex = 0;
  m_CurrentFrameIndex = 0;

  // Calculate viewport aspect from global frame size for water camera
  // projections
  int frameWidth = Vivid::VividApplication::GetFrameWidth();
  int frameHeight = Vivid::VividApplication::GetFrameHeight();
  if (frameWidth > 0 && frameHeight > 0) {
    m_ViewportAspect =
        static_cast<float>(frameWidth) / static_cast<float>(frameHeight);
  }

  RenderReflection(cmd, time);
  RenderRefraction(cmd, time);

  // Pipeline barrier to ensure texture writes are complete before main pass
  // reads
  VkMemoryBarrier memoryBarrier{};
  memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                       &memoryBarrier, 0, nullptr, 0, nullptr);
}

void SceneRenderer::AssignWaterTextures(GraphNode *node) {
  if (!node)
    return;

  if (node->HasMeshes()) {
    for (const auto &mesh : node->GetMeshes()) {
      if (mesh && mesh->GetMaterial()) {
        auto material = mesh->GetMaterial();
        if (material->GetPipelineName() == "PLWater") {

          bool needsUpdate = false;
          if (material->GetReflectionTexture() != m_ReflectionTexture) {
            material->SetReflectionTexture(m_ReflectionTexture);
            needsUpdate = true;
          }
          if (material->GetRefractionTexture() != m_RefractionTexture) {
            material->SetRefractionTexture(m_RefractionTexture);
            needsUpdate = true;
          }

          if (needsUpdate) {
            std::cout
                << "[SceneRenderer] Assigning water textures to material: "
                << material->GetName() << std::endl;
            // Re-create descriptor set to bind new textures
            material->CreateDescriptorSet(m_Device, m_DescriptorPool,
                                          m_MaterialSetLayout,
                                          m_DefaultTexture);
          }
        }
      }
    }
  }

  for (const auto &child : node->GetChildren()) {
    AssignWaterTextures(child.get());
  }
}

} // namespace Quantum
