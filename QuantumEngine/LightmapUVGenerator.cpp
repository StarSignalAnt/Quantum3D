#include "LightmapUVGenerator.h"
#include "include/xatlas/xatlas.h"
#include <iostream>

namespace Quantum {

bool LightmapUVGenerator::GenerateUV2(Mesh3D *mesh, int resolution,
                                      const Settings &settings,
                                      ProgressCallback callback) {
  if (!mesh) {
    m_LastError = "Mesh is null";
    return false;
  }

  const auto &vertices = mesh->GetVertices();
  const auto &triangles = mesh->GetTriangles();

  if (vertices.empty() || triangles.empty()) {
    m_LastError = "Mesh has no geometry";
    return false;
  }

  std::cout << "[LightmapUVGenerator] Starting UV generation for mesh: "
            << mesh->GetName() << std::endl;
  std::cout << "[LightmapUVGenerator] Vertices: " << vertices.size()
            << ", Triangles: " << triangles.size() << std::endl;

  // Create xatlas atlas
  xatlas::Atlas *atlas = xatlas::Create();
  if (!atlas) {
    m_LastError = "Failed to create xatlas atlas";
    return false;
  }

  // Prepare mesh data for xatlas
  xatlas::MeshDecl meshDecl;

  // Position data
  meshDecl.vertexCount = static_cast<uint32_t>(vertices.size());
  meshDecl.vertexPositionData = &vertices[0].position;
  meshDecl.vertexPositionStride = sizeof(Vertex3D);

  // Normal data (optional but improves quality)
  meshDecl.vertexNormalData = &vertices[0].normal;
  meshDecl.vertexNormalStride = sizeof(Vertex3D);

  // UV data (can help with chart boundaries)
  meshDecl.vertexUvData = &vertices[0].uv;
  meshDecl.vertexUvStride = sizeof(Vertex3D);

  // Index data
  std::vector<uint32_t> indices;
  indices.reserve(triangles.size() * 3);
  for (const auto &tri : triangles) {
    indices.push_back(tri.v0);
    indices.push_back(tri.v1);
    indices.push_back(tri.v2);
  }

  meshDecl.indexCount = static_cast<uint32_t>(indices.size());
  meshDecl.indexData = indices.data();
  meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

  // Add mesh to atlas
  xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl);
  if (error != xatlas::AddMeshError::Success) {
    m_LastError = "Failed to add mesh to atlas: " +
                  std::string(xatlas::StringForEnum(error));
    xatlas::Destroy(atlas);
    return false;
  }

  if (callback)
    callback(0.1f);

  // Configure chart options
  xatlas::ChartOptions chartOptions;
  chartOptions.maxChartArea = settings.maxChartArea;
  chartOptions.maxBoundaryLength = settings.maxBoundaryLength;
  chartOptions.normalDeviationWeight = settings.normalDeviationWeight;
  chartOptions.roundnessWeight = settings.roundnessWeight;
  chartOptions.straightnessWeight = settings.straightnessWeight;
  chartOptions.normalSeamWeight = settings.normalSeamWeight;
  chartOptions.textureSeamWeight = settings.textureSeamWeight;

  // Configure pack options
  xatlas::PackOptions packOptions;
  packOptions.padding = settings.padding;
  packOptions.resolution = resolution;
  packOptions.texelsPerUnit = 0.0f; // Auto-calculate
  packOptions.bilinear = true;
  packOptions.blockAlign = true;
  packOptions.bruteForce = false;

  // Generate atlas (this does the heavy lifting)
  std::cout << "[LightmapUVGenerator] Generating atlas..." << std::endl;
  xatlas::Generate(atlas, chartOptions, packOptions);

  if (callback)
    callback(0.8f);

  std::cout << "[LightmapUVGenerator] Atlas generated: " << atlas->width << "x"
            << atlas->height << std::endl;
  std::cout << "[LightmapUVGenerator] Charts: " << atlas->chartCount
            << ", Atlases: " << atlas->atlasCount << std::endl;

  // Check if we got valid output
  if (atlas->meshCount == 0 || atlas->meshes[0].vertexCount == 0) {
    m_LastError = "xatlas generated empty atlas";
    xatlas::Destroy(atlas);
    return false;
  }

  const xatlas::Mesh &outputMesh = atlas->meshes[0];

  // The output mesh may have more vertices than input (due to seams)
  // We need to rebuild the mesh with the new vertex layout
  std::vector<Vertex3D> newVertices;
  newVertices.reserve(outputMesh.vertexCount);

  for (uint32_t i = 0; i < outputMesh.vertexCount; i++) {
    const xatlas::Vertex &xv = outputMesh.vertexArray[i];

    // Copy original vertex data
    Vertex3D v = vertices[xv.xref];

    // Set lightmap UV (normalized to atlas size)
    if (atlas->width > 0 && atlas->height > 0) {
      v.uv2.x = xv.uv[0] / static_cast<float>(atlas->width);
      v.uv2.y = xv.uv[1] / static_cast<float>(atlas->height);
    } else {
      v.uv2 = glm::vec2(0.0f);
    }

    newVertices.push_back(v);
  }

  // Rebuild triangles with new indices
  std::vector<Triangle> newTriangles;
  newTriangles.reserve(outputMesh.indexCount / 3);

  for (uint32_t i = 0; i < outputMesh.indexCount; i += 3) {
    newTriangles.emplace_back(outputMesh.indexArray[i],
                              outputMesh.indexArray[i + 1],
                              outputMesh.indexArray[i + 2]);
  }

  // Update the mesh
  mesh->SetVertices(newVertices);
  mesh->SetTriangles(newTriangles);
  mesh->SetHasLightmapUVs(true);
  mesh->MarkGeometryDirty();

  std::cout << "[LightmapUVGenerator] Updated mesh: " << newVertices.size()
            << " vertices, " << newTriangles.size() << " triangles"
            << std::endl;

  // Cleanup
  xatlas::Destroy(atlas);

  if (callback)
    callback(1.0f);

  return true;
}

} // namespace Quantum
