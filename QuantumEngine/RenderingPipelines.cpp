#include "RenderingPipelines.h"
#include "VividPipeline.h"
#include <iostream>
#include <stdexcept>

namespace Quantum {

RenderingPipelines &RenderingPipelines::Get() {
  static RenderingPipelines instance;
  return instance;
}

RenderingPipelines::~RenderingPipelines() { Shutdown(); }

void RenderingPipelines::Initialize(
    Vivid::VividDevice *device, VkRenderPass renderPass,
    const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts) {
  std::cout << "[RenderingPipelines] Initialize() called" << std::endl;

  if (m_Initialized) {
    // If already initialized, check if we need to update the render pass
    // (happens after swapchain recreation where InvalidatePipelines was called)
    if (m_RenderPass == VK_NULL_HANDLE && renderPass != VK_NULL_HANDLE) {
      std::cout << "[RenderingPipelines] Re-initializing with new render pass"
                << std::endl;
      m_Device = device;
      m_RenderPass = renderPass;
      m_DescriptorSetLayouts = descriptorSetLayouts;
      return;
    }
    std::cout << "[RenderingPipelines] Already initialized, skipping"
              << std::endl;
    return;
  }

  m_Device = device;
  m_RenderPass = renderPass;
  m_DescriptorSetLayouts = descriptorSetLayouts;
  m_Initialized = true;

  std::cout << "[RenderingPipelines] Initialized successfully" << std::endl;
}

void RenderingPipelines::Shutdown() {
  std::cout << "[RenderingPipelines] Shutdown() called" << std::endl;

  // Pipelines are cleaned up via unique_ptr destruction
  m_Pipelines.clear();
  m_Device = nullptr;
  m_RenderPass = VK_NULL_HANDLE;
  m_RenderPass = VK_NULL_HANDLE;
  m_DescriptorSetLayouts.clear();
  m_Initialized = false;

  std::cout << "[RenderingPipelines] Shutdown complete" << std::endl;
}

void RenderingPipelines::InvalidatePipelines() {
  std::cout << "[RenderingPipelines] InvalidatePipelines() called" << std::endl;

  // Only destroy created pipeline objects, keep registrations intact
  for (auto &pair : m_Pipelines) {
    pair.second.pipeline.reset();
  }

  // Clear the render pass since it's now invalid
  m_RenderPass = VK_NULL_HANDLE;

  std::cout << "[RenderingPipelines] Pipelines invalidated (registrations kept)"
            << std::endl;
}

void RenderingPipelines::RegisterPipeline(const std::string &name,
                                          const std::string &vertShaderPath,
                                          const std::string &fragShaderPath,
                                          const Vivid::BlendConfig &blendConfig,
                                          Vivid::PipelineType pipelineType) {
  std::cout << "[RenderingPipelines] RegisterPipeline() called for '" << name
            << "'" << std::endl;
  std::cout << "[RenderingPipelines]   Vertex shader: " << vertShaderPath
            << std::endl;
  std::cout << "[RenderingPipelines]   Fragment shader: " << fragShaderPath
            << std::endl;
  std::cout << "[RenderingPipelines]   Pipeline type: "
            << (pipelineType == Vivid::PipelineType::Mesh3D ? "Mesh3D"
                                                            : "Sprite2D")
            << std::endl;

  if (m_Pipelines.find(name) != m_Pipelines.end()) {
    std::cout << "[RenderingPipelines] Pipeline '" << name
              << "' already registered, updating" << std::endl;
    // Already registered - update shader paths
    m_Pipelines[name].vertShaderPath = vertShaderPath;
    m_Pipelines[name].fragShaderPath = fragShaderPath;
    m_Pipelines[name].blendConfig = blendConfig;
    m_Pipelines[name].pipelineType = pipelineType;
    m_Pipelines[name].pipeline.reset(); // Force recreation on next GetPipeline
    return;
  }

  PipelineInfo info;
  info.vertShaderPath = vertShaderPath;
  info.fragShaderPath = fragShaderPath;
  info.blendConfig = blendConfig;
  info.pipelineType = pipelineType;
  info.pipeline = nullptr;
  m_Pipelines[name] = std::move(info);

  std::cout << "[RenderingPipelines] Pipeline '" << name
            << "' registered successfully" << std::endl;
}

Vivid::VividPipeline *RenderingPipelines::GetPipeline(const std::string &name) {
  if (!m_Initialized) {
    std::cerr << "[RenderingPipelines] ERROR: Not initialized!" << std::endl;
    throw std::runtime_error(
        "RenderingPipelines not initialized! Call Initialize() first.");
  }

  auto it = m_Pipelines.find(name);
  if (it == m_Pipelines.end()) {
    std::cerr << "[RenderingPipelines] ERROR: Pipeline '" << name
              << "' not registered!" << std::endl;
    throw std::runtime_error(
        "Pipeline '" + name +
        "' not registered! Call RegisterPipeline() first.");
  }

  // Lazy creation - create pipeline on first access
  if (!it->second.pipeline) {
    std::cout << "[RenderingPipelines] Creating pipeline '" << name << "'..."
              << std::endl;
    std::cout << "[RenderingPipelines]   Loading vertex shader: "
              << it->second.vertShaderPath << std::endl;
    std::cout << "[RenderingPipelines]   Loading fragment shader: "
              << it->second.fragShaderPath << std::endl;
    std::cout << "[RenderingPipelines]   Pipeline type: "
              << (it->second.pipelineType == Vivid::PipelineType::Mesh3D
                      ? "Mesh3D"
                      : "Sprite2D")
              << std::endl;

    try {
      it->second.pipeline = std::make_unique<Vivid::VividPipeline>(
          m_Device, it->second.vertShaderPath, it->second.fragShaderPath,
          m_DescriptorSetLayouts, m_RenderPass, it->second.blendConfig,
          it->second.pipelineType);
      it->second.pipeline->SetName(name);

      std::cout << "[RenderingPipelines] Pipeline '" << name
                << "' created successfully" << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[RenderingPipelines] ERROR: Failed to create pipeline '"
                << name << "': " << e.what() << std::endl;
      throw;
    }
  }

  return it->second.pipeline.get();
}

bool RenderingPipelines::HasPipeline(const std::string &name) const {
  return m_Pipelines.find(name) != m_Pipelines.end();
}

std::vector<std::string> RenderingPipelines::GetPipelineNames() const {
  std::vector<std::string> names;
  names.reserve(m_Pipelines.size());
  for (const auto &pair : m_Pipelines) {
    names.push_back(pair.first);
  }
  return names;
}

} // namespace Quantum
