#pragma once
#include "VividDevice.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <array>
#include <vulkan/vulkan.h>

namespace Quantum {

/// <summary>
/// Manages a cube shadow map for omnidirectional point light shadows.
/// Creates a cube map texture with 6 faces for capturing depth from all
/// directions.
/// </summary>
class PointShadowMap {
public:
  static constexpr uint32_t NUM_FACES = 6;

  PointShadowMap() = default;
  ~PointShadowMap();

  // Initialize shadow map resources
  void Initialize(Vivid::VividDevice *device, uint32_t resolution = 1024);

  // Cleanup resources
  void Shutdown();

  // Get resolution
  uint32_t GetResolution() const { return m_Resolution; }

  // Get view matrix for a specific cube face from light position
  glm::mat4 GetFaceViewMatrix(const glm::vec3 &lightPos, uint32_t face) const;

  // Get projection matrix (90 degree FOV for cube face)
  glm::mat4 GetProjectionMatrix() const;

  // Get combined view-projection matrix for a face
  glm::mat4 GetLightSpaceMatrix(const glm::vec3 &lightPos, uint32_t face) const;

  // Get the cube image view for shader sampling
  VkImageView GetCubeImageView() const { return m_CubeImageView; }

  // Get the sampler for shader sampling
  VkSampler GetSampler() const { return m_Sampler; }

  // Get individual face image view (for framebuffer attachment)
  VkImageView GetFaceImageView(uint32_t face) const;

  // Get framebuffer for a specific face
  VkFramebuffer GetFramebuffer(uint32_t face) const;

  // Get far plane distance (for depth linearization in shader)
  float GetFarPlane() const { return m_FarPlane; }
  void SetFarPlane(float farPlane) { m_FarPlane = farPlane; }

  // Check if initialized
  bool IsInitialized() const { return m_Initialized; }

  // Get render pass (needed for framebuffer compatibility)
  VkRenderPass GetRenderPass() const { return m_RenderPass; }

private:
  void CreateCubeImage();
  void CreateCubeImageView();
  void CreateFaceImageViews();
  void CreateSampler();
  void CreateRenderPass();
  void CreateFramebuffers();
  void TransitionToShaderReadable(); // Transition image to valid layout

  Vivid::VividDevice *m_Device = nullptr;
  uint32_t m_Resolution = 1024;
  float m_FarPlane = 100.0f; // Default far plane for shadow
  float m_NearPlane = 0.003f;
  bool m_Initialized = false;

  // Cube map image
  VkImage m_CubeImage = VK_NULL_HANDLE;
  VkDeviceMemory m_CubeMemory = VK_NULL_HANDLE;

  // Cube image view (for shader sampling)
  VkImageView m_CubeImageView = VK_NULL_HANDLE;

  // Per-face image views (for framebuffer attachment)
  std::array<VkImageView, NUM_FACES> m_FaceImageViews{};

  // Sampler for shadow comparison
  VkSampler m_Sampler = VK_NULL_HANDLE;

  // Render pass for shadow depth
  VkRenderPass m_RenderPass = VK_NULL_HANDLE;

  // Framebuffers (one per face)
  std::array<VkFramebuffer, NUM_FACES> m_Framebuffers{};
};

} // namespace Quantum
