#include "LightmapBaker.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

namespace Quantum {

LightmapBaker::LightmapBaker() {}

bool LightmapBaker::Bake(Vivid::VividDevice *device,
                         std::shared_ptr<SceneGraph> sceneGraph,
                         const BakeSettings &settings,
                         ProgressCallback callback) {
  if (!sceneGraph) {
    m_LastError = "SceneGraph is null";
    return false;
  }

  m_BakedLightmaps.clear();

  if (callback)
    callback(0.0f, "Collecting scene data...");

  // Collect all lights and meshes from scene
  auto lights = CollectLights(sceneGraph);
  m_AllMeshes = CollectMeshes(sceneGraph);

  std::cout << "[LightmapBaker] Found " << lights.size() << " lights and "
            << m_AllMeshes.size() << " meshes" << std::endl;

  if (lights.empty()) {
    m_LastError = "No lights in scene";
    return false;
  }

  if (m_AllMeshes.empty()) {
    m_LastError = "No meshes in scene";
    return false;
  }

  float progressPerMesh = 0.9f / static_cast<float>(m_AllMeshes.size());
  float currentProgress = 0.05f;

  // Process each mesh
  for (size_t meshIdx = 0; meshIdx < m_AllMeshes.size(); meshIdx++) {
    MeshInstance &instance = m_AllMeshes[meshIdx];

    if (callback) {
      callback(currentProgress, "Baking mesh: " + instance.mesh->GetName() +
                                    " (" + std::to_string(meshIdx + 1) + "/" +
                                    std::to_string(m_AllMeshes.size()) + ")");
    }

    std::cout << "[LightmapBaker] Processing mesh: " << instance.mesh->GetName()
              << std::endl;

    // Step 1: Ensure mesh has lightmap UVs
    if (!EnsureUV2(device, instance.mesh, settings.resolution)) {
      std::cout << "[LightmapBaker] Warning: Failed to generate UV2 for "
                << instance.mesh->GetName() << ", skipping" << std::endl;
      continue;
    }

    // Step 2: Rasterize mesh to get texel world positions
    std::vector<LightmapTexel> texels;
    RasterizeMesh(instance.mesh, instance.worldMatrix, settings.resolution,
                  texels);

    std::cout << "[LightmapBaker] Rasterized " << texels.size() << " texels"
              << std::endl;

    // Count valid texels
    int validCount = 0;
    for (const auto &t : texels) {
      if (t.valid)
        validCount++;
    }
    std::cout << "[LightmapBaker] Valid texels: " << validCount << std::endl;

    // Step 3: Compute direct lighting (GPU if available, CPU fallback)
    std::vector<glm::vec3> lighting(texels.size(), glm::vec3(0.0f));

    bool gpuSuccess = false;
    if (settings.useGPU && m_CLLightmapper.IsValid()) {
      if (callback)
        callback(currentProgress,
                 "Baking " + instance.mesh->GetName() + " (GPU)...");
      gpuSuccess = ComputeDirectLightingGPU(texels, lights, lighting, settings);
      if (!gpuSuccess) {
        std::cout << "[LightmapBaker] GPU baking failed, falling back to CPU"
                  << std::endl;
      }
    }

    if (!gpuSuccess) {
      if (callback)
        callback(currentProgress,
                 "Baking " + instance.mesh->GetName() + " (CPU)...");
      ComputeDirectLighting(texels, lights, lighting, settings);
    }

    // Step 4: Compute global illumination if enabled
    if (settings.enableGI && settings.giBounces > 0) {
      if (callback)
        callback(currentProgress,
                 "Baking GI (" + instance.mesh->GetName() + ")...");
      ComputeGlobalIllumination(texels, lights, lighting, settings);
    }

    // Store the baked lightmap
    BakedLightmap baked;
    baked.width = settings.resolution;
    baked.height = settings.resolution;
    baked.meshName = instance.mesh->GetName();
    baked.pixels = std::move(lighting);

    // Create GPU texture and assign to mesh
    auto texture = CreateLightmapTexture(device, baked);
    if (texture) {
      instance.mesh->SetLightmap(texture);

      // Also set lightmap on material's refraction slot (binding 5 in shader)
      // This ensures the descriptor set picks it up when rendered
      auto material = instance.mesh->GetMaterial();
      if (material) {
        material->SetRefractionTexture(texture);
        // Mark descriptor as needing update
        material->InvalidateDescriptorSet();
      }
    }

    m_BakedLightmaps.push_back(std::move(baked));
    currentProgress += progressPerMesh;
  }

  if (callback)
    callback(1.0f, "Baking complete!");

  std::cout << "[LightmapBaker] Baking complete! Generated "
            << m_BakedLightmaps.size() << " lightmaps" << std::endl;

  return true;
}

bool LightmapBaker::EnsureUV2(Vivid::VividDevice *device, Mesh3D *mesh,
                              int resolution) {
  if (mesh->HasLightmapUVs()) {
    std::cout << "[LightmapBaker] Mesh already has UV2" << std::endl;
    return true;
  }

  LightmapUVGenerator::Settings uvSettings;
  bool success = m_UVGenerator.GenerateUV2(mesh, resolution, uvSettings);

  if (success) {
    // CRITICAL: Rebuild GPU buffers after UV generation modifies mesh geometry
    // xatlas may add vertices due to seam splitting, so we must recreate
    // buffers
    std::cout << "[LightmapBaker] Rebuilding GPU buffers after UV2 generation"
              << std::endl;
    mesh->Finalize(device);
  }

  return success;
}

void LightmapBaker::RasterizeMesh(const Mesh3D *mesh,
                                  const glm::mat4 &worldMatrix, int resolution,
                                  std::vector<LightmapTexel> &texels) {
  texels.resize(resolution * resolution);

  const auto &vertices = mesh->GetVertices();
  const auto &triangles = mesh->GetTriangles();

  // Normal matrix for transforming normals
  glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));

  // For each triangle, rasterize it to the lightmap
  for (size_t triIdx = 0; triIdx < triangles.size(); triIdx++) {
    const Triangle &tri = triangles[triIdx];

    if (tri.v0 >= vertices.size() || tri.v1 >= vertices.size() ||
        tri.v2 >= vertices.size()) {
      continue;
    }

    const Vertex3D &v0 = vertices[tri.v0];
    const Vertex3D &v1 = vertices[tri.v1];
    const Vertex3D &v2 = vertices[tri.v2];

    // Get UV2 coordinates (lightmap space)
    glm::vec2 uv0 = v0.uv2;
    glm::vec2 uv1 = v1.uv2;
    glm::vec2 uv2 = v2.uv2;

    // Convert to pixel coordinates
    glm::vec2 p0 = uv0 * static_cast<float>(resolution);
    glm::vec2 p1 = uv1 * static_cast<float>(resolution);
    glm::vec2 p2 = uv2 * static_cast<float>(resolution);

    // Compute bounding box
    int minX =
        std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
    int maxX =
        std::min(resolution - 1,
                 static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
    int minY =
        std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
    int maxY =
        std::min(resolution - 1,
                 static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));

    // Edge functions for barycentric coordinates
    auto edgeFunc = [](const glm::vec2 &a, const glm::vec2 &b,
                       const glm::vec2 &c) {
      return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
    };

    float area = edgeFunc(p0, p1, p2);
    if (std::abs(area) < 0.0001f)
      continue; // Degenerate triangle

    // Rasterize
    for (int y = minY; y <= maxY; y++) {
      for (int x = minX; x <= maxX; x++) {
        glm::vec2 p(x + 0.5f, y + 0.5f);

        // Barycentric coordinates
        float w0 = edgeFunc(p1, p2, p) / area;
        float w1 = edgeFunc(p2, p0, p) / area;
        float w2 = edgeFunc(p0, p1, p) / area;

        // Check if point is inside triangle
        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
          int idx = y * resolution + x;

          // Interpolate world position
          glm::vec3 localPos =
              w0 * v0.position + w1 * v1.position + w2 * v2.position;
          glm::vec3 worldPos =
              glm::vec3(worldMatrix * glm::vec4(localPos, 1.0f));

          // Interpolate and transform normal
          glm::vec3 localNormal =
              glm::normalize(w0 * v0.normal + w1 * v1.normal + w2 * v2.normal);
          glm::vec3 worldNormal = glm::normalize(normalMatrix * localNormal);

          texels[idx].worldPos = worldPos;
          texels[idx].worldNormal = worldNormal;
          texels[idx].valid = true;
          texels[idx].triangleIndex = static_cast<int>(triIdx);
          texels[idx].barycentrics = glm::vec2(w0, w1);
        }
      }
    }
  }
}

void LightmapBaker::ComputeDirectLighting(
    const std::vector<LightmapTexel> &texels,
    const std::vector<LightNode *> &lights, std::vector<glm::vec3> &lighting,
    const BakeSettings &settings) {

  for (size_t i = 0; i < texels.size(); i++) {
    if (!texels[i].valid)
      continue;

    const glm::vec3 &texelPos = texels[i].worldPos;
    const glm::vec3 &texelNormal = texels[i].worldNormal;

    glm::vec3 totalLight(0.0f);

    for (LightNode *light : lights) {
      if (light->GetType() != LightNode::LightType::Point)
        continue; // Only point lights for now

      glm::vec3 lightPos = light->GetWorldPosition();
      glm::vec3 lightColor = light->GetColor();
      float lightRange = light->GetRange();

      // Direction and distance to light
      glm::vec3 toLight = lightPos - texelPos;
      float distance = glm::length(toLight);

      // Skip if out of range
      if (lightRange > 0 && distance > lightRange)
        continue;

      glm::vec3 lightDir = toLight / distance;

      // N dot L
      float NdotL = glm::max(0.0f, glm::dot(texelNormal, lightDir));
      if (NdotL <= 0.0f)
        continue;

      // Attenuation
      float attenuation = 1.0f / (distance * distance + 0.001f);

      // Range falloff
      float rangeFactor = 1.0f;
      if (lightRange > 0) {
        rangeFactor = glm::max(0.0f, 1.0f - distance / lightRange);
      }

      // Shadow factor
      float shadow = 1.0f;
      if (settings.enableShadows) {
        shadow = TraceShadowRay(texelPos + texelNormal * 0.01f, lightPos);
      }

      totalLight += lightColor * NdotL * attenuation * rangeFactor * shadow;
    }

    lighting[i] = totalLight;
  }
}

float LightmapBaker::TraceShadowRay(const glm::vec3 &texelPos,
                                    const glm::vec3 &lightPos) {
  // Cast shadow ray from texel to light
  // NOTE: Intersections uses (startPoint, endPoint), not (origin, direction)

  for (const MeshInstance &instance : m_AllMeshes) {
    // Use Intersections to check if ray hits any mesh
    CastResult result = m_Intersections.CastMesh(instance.worldMatrix, texelPos,
                                                 lightPos, instance.mesh);

    if (result.Hit) {
      // Check if hit is between texel and light (not behind light)
      float distToLight = glm::length(lightPos - texelPos);
      if (result.Distance > 0.001f && result.Distance < distToLight) {
        return 0.0f; // In shadow
      }
    }
  }

  return 1.0f; // Fully lit
}

// Compute global illumination (CPU/GPU Dispatch)
void LightmapBaker::ComputeGlobalIllumination(
    std::vector<LightmapTexel> &texels, const std::vector<LightNode *> &lights,
    std::vector<glm::vec3> &lighting, const BakeSettings &settings) {

  // Try GPU first
  if (settings.useGPU && m_CLLightmapper.IsValid()) {
    if (ComputeGlobalIlluminationGPU(texels, lights, lighting, settings)) {
      return;
    }
    std::cout << "[LightmapBaker] GPU GI failed, falling back to CPU"
              << std::endl;
  }

  // CPU Fallback
  std::cout << "[LightmapBaker] Computing GI (CPU) with " << settings.giBounces
            << " bounces..." << std::endl;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  // Copy current lighting as input radiance for bounces
  std::vector<glm::vec3> incomingRadiance = lighting;

  for (int bounce = 0; bounce < settings.giBounces; bounce++) {
    std::cout << "[LightmapBaker] GI Bounce " << (bounce + 1) << std::endl;

    std::vector<glm::vec3> bounceLight(texels.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < texels.size(); i++) {
      if (!texels[i].valid)
        continue;

      glm::vec3 indirectLight(0.0f);

      // Sample hemisphere around normal
      for (int s = 0; s < settings.giSamples; s++) {
        glm::vec3 sampleDir =
            SampleHemisphere(texels[i].worldNormal, dist(gen), dist(gen));

        // Trace ray in sample direction
        // For simplicity, trace a fixed distance
        float traceDistance = 10.0f;
        glm::vec3 endPoint = texels[i].worldPos + sampleDir * traceDistance;

        for (const MeshInstance &instance : m_AllMeshes) {
          CastResult result = m_Intersections.CastMesh(
              instance.worldMatrix,
              texels[i].worldPos + texels[i].worldNormal * 0.01f, endPoint,
              instance.mesh);

          if (result.Hit && result.Distance > 0.01f) {
            // We hit something - sample its lighting
            // For now, use average scene radiance as approximation
            float NdotL =
                glm::max(0.0f, glm::dot(texels[i].worldNormal, sampleDir));
            indirectLight += incomingRadiance[i] * NdotL /
                             static_cast<float>(settings.giSamples);
            break;
          }
        }
      }

      bounceLight[i] = indirectLight * settings.giIntensity;
    }

    // Accumulate bounce lighting
    for (size_t i = 0; i < texels.size(); i++) {
      lighting[i] += bounceLight[i];
    }

    // Update incoming radiance for next bounce
    for (size_t i = 0; i < texels.size(); i++) {
      // Ideally we would update incomingRadiance with the new bounce light at
      // HIT POINTS But CPU implementation here is limited.
      incomingRadiance[i] = bounceLight[i]; // Approximation
    }
  }
}

// Global Illumination GPU implementation
bool LightmapBaker::ComputeGlobalIlluminationGPU(
    const std::vector<LightmapTexel> &texels,
    const std::vector<LightNode *> &lights, std::vector<glm::vec3> &lighting,
    const BakeSettings &settings) {

  if (!m_CLLightmapper.IsValid())
    return false;

  // Convert texels to GPU format
  std::vector<CLLightmapper::TexelData> gpuTexels(texels.size());
  for (size_t i = 0; i < texels.size(); i++) {
    gpuTexels[i].worldPos = glm::vec4(texels[i].worldPos, 0.0f);
    gpuTexels[i].normal = glm::vec4(texels[i].worldNormal, 0.0f);
    gpuTexels[i].valid = texels[i].valid ? 1 : 0;
  }

  // Convert lights to GPU format
  std::vector<CLLightmapper::LightData> gpuLights;
  for (auto *light : lights) {
    if (!light)
      continue;
    CLLightmapper::LightData ld;
    ld.positionAndRange =
        glm::vec4(light->GetWorldPosition(), light->GetRange());
    float type = static_cast<float>(static_cast<int>(light->GetType()));
    ld.colorAndType = glm::vec4(light->GetColor(), type);
    glm::mat4 worldMat = light->GetWorldMatrix();
    glm::vec3 dir = glm::normalize(
        glm::vec3(worldMat * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    ld.direction = glm::vec4(dir, 0.0f);
    gpuLights.push_back(ld);
  }

  // Collect scene triangles (might want to cache this in Bake but ok for now)
  std::vector<float> sceneTriangles;
  int numTriangles = 0;
  CollectSceneTriangles(sceneTriangles, numTriangles);

  std::vector<glm::vec3> indirectLighting;

  // Execute GPU Kernel (Single Bounce)
  bool success = m_CLLightmapper.BakeIndirect(
      gpuTexels, gpuLights, sceneTriangles, numTriangles,
      settings.enableShadows, settings.giSamples, settings.giIntensity,
      indirectLighting);

  if (success) {
    // Add indirect lighting to total lighting
    for (size_t i = 0; i < lighting.size(); i++) {
      if (i < indirectLighting.size()) {
        lighting[i] += indirectLighting[i];
      }
    }
    std::cout << "[LightmapBaker] GPU GI finished" << std::endl;
  }

  return success;
}

glm::vec3 LightmapBaker::SampleHemisphere(const glm::vec3 &normal, float u1,
                                          float u2) {
  // Cosine-weighted hemisphere sampling
  float r = std::sqrt(u1);
  float theta = 2.0f * 3.14159265f * u2;

  float x = r * std::cos(theta);
  float y = r * std::sin(theta);
  float z = std::sqrt(1.0f - u1);

  // Create orthonormal basis around normal
  glm::vec3 up =
      std::abs(normal.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
  glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
  glm::vec3 bitangent = glm::cross(normal, tangent);

  return glm::normalize(tangent * x + bitangent * y + normal * z);
}

std::shared_ptr<Vivid::Texture2D>
LightmapBaker::CreateLightmapTexture(Vivid::VividDevice *device,
                                     const BakedLightmap &baked) {
  if (baked.pixels.empty() || baked.width == 0 || baked.height == 0) {
    return nullptr;
  }

  // Convert float RGB to RGBA8
  std::vector<unsigned char> rgba(baked.width * baked.height * 4);

  for (size_t i = 0; i < baked.pixels.size(); i++) {
    glm::vec3 color = baked.pixels[i];

    // Reinhard tonemapping to handle HDR values
    // This maps [0, infinity) to [0, 1) smoothly
    color = color / (color + glm::vec3(1.0f));

    // Now clamp (should already be in [0,1) but just to be safe)
    color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));

    rgba[i * 4 + 0] = static_cast<unsigned char>(color.r * 255.0f);
    rgba[i * 4 + 1] = static_cast<unsigned char>(color.g * 255.0f);
    rgba[i * 4 + 2] = static_cast<unsigned char>(color.b * 255.0f);
    rgba[i * 4 + 3] = 255;
  }

  // Use Texture2D constructor with raw pixel data
  return std::make_shared<Vivid::Texture2D>(device, rgba.data(), baked.width,
                                            baked.height, 4);
}

std::vector<LightNode *>
LightmapBaker::CollectLights(std::shared_ptr<SceneGraph> sceneGraph) {
  std::vector<LightNode *> lights;

  std::function<void(GraphNode *)> traverse = [&](GraphNode *node) {
    if (auto light = dynamic_cast<LightNode *>(node)) {
      lights.push_back(light);
    }
    for (const auto &child : node->GetChildren()) {
      traverse(child.get());
    }
  };

  traverse(sceneGraph->GetRoot());
  return lights;
}

std::vector<LightmapBaker::MeshInstance>
LightmapBaker::CollectMeshes(std::shared_ptr<SceneGraph> sceneGraph) {
  std::vector<MeshInstance> meshes;

  std::function<void(GraphNode *)> traverse = [&](GraphNode *node) {
    if (node->HasMeshes()) {
      glm::mat4 worldMatrix = node->GetWorldMatrix();
      for (const auto &mesh : node->GetMeshes()) {
        meshes.push_back({mesh.get(), worldMatrix, node});
      }
    }
    for (const auto &child : node->GetChildren()) {
      traverse(child.get());
    }
  };

  traverse(sceneGraph->GetRoot());
  return meshes;
}

// Collect all scene triangles in world space for GPU shadow tracing
void LightmapBaker::CollectSceneTriangles(std::vector<float> &triangles,
                                          int &numTriangles) {
  triangles.clear();
  numTriangles = 0;

  for (const auto &instance : m_AllMeshes) {
    const auto &vertices = instance.mesh->GetVertices();
    const auto &tris = instance.mesh->GetTriangles();

    for (const auto &tri : tris) {
      // Transform vertices to world space
      glm::vec3 v0 = glm::vec3(instance.worldMatrix *
                               glm::vec4(vertices[tri.v0].position, 1.0f));
      glm::vec3 v1 = glm::vec3(instance.worldMatrix *
                               glm::vec4(vertices[tri.v1].position, 1.0f));
      glm::vec3 v2 = glm::vec3(instance.worldMatrix *
                               glm::vec4(vertices[tri.v2].position, 1.0f));

      // Add all 9 floats (3 vertices * 3 components)
      triangles.push_back(v0.x);
      triangles.push_back(v0.y);
      triangles.push_back(v0.z);
      triangles.push_back(v1.x);
      triangles.push_back(v1.y);
      triangles.push_back(v1.z);
      triangles.push_back(v2.x);
      triangles.push_back(v2.y);
      triangles.push_back(v2.z);
      numTriangles++;
    }
  }

  std::cout << "[LightmapBaker] Collected " << numTriangles
            << " scene triangles for GPU shadow testing" << std::endl;
}

// Compute direct lighting using GPU (OpenCL)
bool LightmapBaker::ComputeDirectLightingGPU(
    const std::vector<LightmapTexel> &texels,
    const std::vector<LightNode *> &lights, std::vector<glm::vec3> &lighting,
    const BakeSettings &settings) {
  if (!m_CLLightmapper.IsValid()) {
    return false;
  }

  // Convert texels to GPU format (16-byte aligned)
  std::vector<CLLightmapper::TexelData> gpuTexels(texels.size());
  for (size_t i = 0; i < texels.size(); i++) {
    gpuTexels[i].worldPos = glm::vec4(texels[i].worldPos, 0.0f);
    gpuTexels[i].normal = glm::vec4(texels[i].worldNormal, 0.0f);
    gpuTexels[i].valid = texels[i].valid ? 1 : 0;
    // Padding is handled by default initialization or memset if needed, but
    // vector alloc is safe
  }

  // Convert lights to GPU format (16-byte aligned)
  std::vector<CLLightmapper::LightData> gpuLights;
  for (auto *light : lights) {
    if (!light)
      continue;

    CLLightmapper::LightData ld;
    // Pack position and range
    ld.positionAndRange =
        glm::vec4(light->GetWorldPosition(), light->GetRange());

    // Pack color and type
    float type = static_cast<float>(static_cast<int>(light->GetType()));
    ld.colorAndType = glm::vec4(light->GetColor(), type);

    // Pack direction
    // Directional lights typically point forward (Z+) or -Z depending on engine
    // convention. Calculate direction from world matrix (assuming -Z is
    // forward)
    glm::mat4 worldMat = light->GetWorldMatrix();
    glm::vec3 dir = glm::normalize(
        glm::vec3(worldMat * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    ld.direction = glm::vec4(dir, 0.0f);

    gpuLights.push_back(ld);
  }

  // Collect all scene triangles for shadow testing
  std::vector<float> sceneTriangles;
  int numTriangles = 0;
  CollectSceneTriangles(sceneTriangles, numTriangles);

  // Bake using GPU
  std::vector<glm::vec3> gpuLighting;
  bool success = m_CLLightmapper.BakeLightmap(
      gpuTexels, gpuLights, sceneTriangles, numTriangles,
      settings.enableShadows, gpuLighting);

  if (success) {
    lighting = gpuLighting;
    std::cout << "[LightmapBaker] GPU baking successful" << std::endl;
  }

  return success;
}

} // namespace Quantum
