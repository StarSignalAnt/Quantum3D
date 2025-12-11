#pragma once
#include "RenderingPipelines.h"
#include "Texture2D.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Quantum {

/// <summary>
/// A material defines the visual appearance of a mesh.
/// It references a shared pipeline (shader) and has its own set of textures.
///
/// Multiple meshes can share the same Material instance.
/// Materials with the same pipeline are batched together during rendering.
/// </summary>
class Material {
public:
  Material(const std::string &name = "Material");
  ~Material();

  // Name
  const std::string &GetName() const { return m_Name; }
  void SetName(const std::string &name) { m_Name = name; }

  // Pipeline (shader pair)
  const std::string &GetPipelineName() const { return m_PipelineName; }
  void SetPipeline(const std::string &pipelineName);
  Vivid::VividPipeline *GetPipeline() const;

  // Textures
  void SetTexture(const std::string &slot,
                  std::shared_ptr<Vivid::Texture2D> texture);
  std::shared_ptr<Vivid::Texture2D> GetTexture(const std::string &slot) const;
  bool HasTexture(const std::string &slot) const;
  void RemoveTexture(const std::string &slot);

  // Common texture slots helpers
  void SetAlbedoTexture(std::shared_ptr<Vivid::Texture2D> texture);
  void SetNormalTexture(std::shared_ptr<Vivid::Texture2D> texture);
  void SetMetallicTexture(std::shared_ptr<Vivid::Texture2D> texture);
  void SetRoughnessTexture(std::shared_ptr<Vivid::Texture2D> texture);
  void SetAOTexture(std::shared_ptr<Vivid::Texture2D> texture);
  void SetEmissiveTexture(std::shared_ptr<Vivid::Texture2D> texture);

  // Checks and creates default 1x1 textures for missing PBR slots
  void CheckRequiredTextures(Vivid::VividDevice *device);

  std::shared_ptr<Vivid::Texture2D> GetAlbedoTexture() const;
  std::shared_ptr<Vivid::Texture2D> GetNormalTexture() const;
  std::shared_ptr<Vivid::Texture2D> GetMetallicTexture() const;
  std::shared_ptr<Vivid::Texture2D> GetRoughnessTexture() const;
  std::shared_ptr<Vivid::Texture2D> GetAOTexture() const;
  std::shared_ptr<Vivid::Texture2D> GetEmissiveTexture() const;

  // Get all texture slots
  const std::unordered_map<std::string, std::shared_ptr<Vivid::Texture2D>> &
  GetAllTextures() const {
    return m_Textures;
  }

  // Standard texture slot names
  static constexpr const char *SLOT_ALBEDO = "albedo";
  static constexpr const char *SLOT_NORMAL = "normal";
  static constexpr const char *SLOT_METALLIC = "metallic";
  static constexpr const char *SLOT_ROUGHNESS = "roughness";
  static constexpr const char *SLOT_AO = "ao";
  static constexpr const char *SLOT_EMISSIVE = "emissive";

private:
  std::string m_Name;
  std::string m_PipelineName;
  std::unordered_map<std::string, std::shared_ptr<Vivid::Texture2D>> m_Textures;
};

} // namespace Quantum
