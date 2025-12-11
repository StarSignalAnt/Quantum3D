#include "SceneRenderer.h"
#include "CameraNode.h"
#include "GraphNode.h"
#include "LightNode.h"
#include "Material.h"
#include "Mesh3D.h"
#include "RenderingPipelines.h"
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
  float padding3; // Align to 16 bytes
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

  RenderingPipelines::Get().RegisterPipeline(
      "PLPBR", "engine/shaders/PLPBR.vert.spv", "engine/shaders/PLPBR.frag.spv",
      Vivid::BlendConfig{}, Vivid::PipelineType::Mesh3D);

  if (RenderingPipelines::Get().HasPipeline("PLPBR")) {
    std::cout << "[SceneRenderer] PLPBR pipeline registered successfully"
              << std::endl;
  } else {
    std::cerr << "[SceneRenderer] ERROR: Failed to register PLPBR pipeline!"
              << std::endl;
  }

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

  std::cout << "[SceneRenderer] Resetting uniform buffer..." << std::endl;
  m_UniformBuffer.reset();

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
    }
  }
}

void SceneRenderer::CreateDescriptorSetLayout() {
  std::cout << "[SceneRenderer] CreateDescriptorSetLayout() started"
            << std::endl;

  // Binding 0: Uniform buffer (vertex shader)
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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

  std::array<VkDescriptorSetLayoutBinding, 5> bindings = {
      uboLayoutBinding, albedoBinding, normalBinding, metallicBinding,
      roughnessBinding};

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

  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = 1;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = 4; // 4 PBR textures (Albedo, Norm, Met, Rough)

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 1;

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
  VkDescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = m_UniformBuffer->GetBuffer();
  bufferInfo.offset = 0;
  bufferInfo.range = sizeof(UniformBufferObject);

  VkWriteDescriptorSet uboWrite{};
  uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  uboWrite.dstSet = m_DescriptorSet;
  uboWrite.dstBinding = 0;
  uboWrite.dstArrayElement = 0;
  uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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

  m_UniformBuffer = std::make_unique<Vivid::VividBuffer>(
      m_Device, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_UniformBuffer->Map();

  std::cout << "[SceneRenderer] CreateUniformBuffer() completed successfully"
            << std::endl;
}

void SceneRenderer::RenderScene(VkCommandBuffer cmd, int width, int height) {
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

  // Render the scene
  if (m_SceneGraph && m_SceneGraph->GetRoot()) {
    RenderNode(cmd, m_SceneGraph->GetRoot(), width, height);
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
    ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));

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

    // Use first light from scene graph if available
    if (m_SceneGraph && !m_SceneGraph->GetLights().empty()) {
      auto light = m_SceneGraph->GetLights()[0];
      ubo.lightPos = light->GetWorldPosition();
      ubo.lightColor = light->GetColor();
    } else {
      // Fallback: Light above and to the side
      ubo.lightPos = glm::vec3(3.0f, 8.0f, -2.0f);
      // White light - good intensity for PBR
      ubo.lightColor = glm::vec3(150.0f, 150.0f, 150.0f);
    }
    ubo.padding2 = 0.0f;
    ubo.padding3 = 0.0f;
    ubo.padding3 = 0.0f;

    m_UniformBuffer->WriteToBuffer(&ubo, sizeof(ubo));

    // Log first time we find meshes
    static bool loggedMeshInfo = false;
    if (!loggedMeshInfo) {
      std::cout << "[SceneRenderer] Node '" << node->GetName() << "' has "
                << node->GetMeshCount() << " meshes" << std::endl;
      glm::vec3 pos = node->GetWorldPosition();
      std::cout << "[SceneRenderer] Node world position: (" << pos.x << ", "
                << pos.y << ", " << pos.z << ")" << std::endl;
      loggedMeshInfo = true;
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

          // Fall back to default texture if no albedo
          if (!albedoTexture) {
            albedoTexture = m_DefaultTexture.get();
          }

          // Bind pipeline if it changed
          if (meshPipeline && meshPipeline != m_CurrentPipeline) {
            m_CurrentPipeline = meshPipeline;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              meshPipeline->GetPipeline());

            // Bind descriptor set with the new pipeline's layout
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    meshPipeline->GetPipelineLayout(), 0, 1,
                                    &m_DescriptorSet, 0, nullptr);
          }

          // PBR textures are updated in SetSceneGraph, not during rendering
          // (Vulkan does not allow descriptor updates during command buffer
          // recording)

          mesh->Bind(cmd);
          uint32_t indexCount = static_cast<uint32_t>(mesh->GetIndexCount());
          vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
          m_RenderMeshCount++;

          // Log first mesh details
          static bool loggedFirstMesh = false;
          if (!loggedFirstMesh) {
            std::cout << "[SceneRenderer] First mesh: "
                      << mesh->GetVertexCount() << " vertices, "
                      << mesh->GetIndexCount() << " indices";
            if (material) {
              std::cout << ", pipeline: " << material->GetPipelineName();
            }
            std::cout << std::endl;
            loggedFirstMesh = true;
          }
        } else {
          static bool loggedUnfinalizedMesh = false;
          if (!loggedUnfinalizedMesh) {
            std::cerr << "[SceneRenderer] WARNING: Mesh is not finalized!"
                      << std::endl;
            loggedUnfinalizedMesh = true;
          }
        }
      } else {
        static bool loggedNullMesh = false;
        if (!loggedNullMesh) {
          std::cerr << "[SceneRenderer] WARNING: Null mesh pointer in node!"
                    << std::endl;
          loggedNullMesh = true;
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

  std::cout
      << "[SceneRenderer] About to call vkUpdateDescriptorSets, DescriptorSet: "
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

  // Check if this node has meshes with materials
  for (const auto &mesh : node->GetMeshes()) {
    if (mesh) {
      auto material = mesh->GetMaterial();
      if (material) {
        std::cout << "[SceneRenderer] Found material: " << material->GetName()
                  << std::endl;
        std::cout << "[SceneRenderer] Binding textures from first material..."
                  << std::endl;
        UpdatePBRTextures(material.get());
        return; // Only update with first material found
      }
    }
  }

  // Recursively check children
  for (const auto &child : node->GetChildren()) {
    UpdateFirstMaterialTextures(child.get());
  }
}

} // namespace Quantum
