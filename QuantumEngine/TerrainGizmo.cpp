#include "TerrainGizmo.h"
#include "Intersections.h"
#include "RenderingPipelines.h"
#include "SceneRenderer.h"
#include "TerrainNode.h"
#include <cmath>

namespace Quantum {

TerrainGizmo::TerrainGizmo(Vivid::VividDevice *device) : m_Device(device) {
  m_Intersections = std::make_unique<Intersections>();
  Initialize();
}

TerrainGizmo::~TerrainGizmo() = default;

void TerrainGizmo::Initialize() { RebuildMesh(); }

void TerrainGizmo::RebuildMesh() {
  m_Mesh = std::make_shared<Mesh3D>("TerrainBrush");

  std::vector<Vertex3D> vertices;
  std::vector<Triangle> triangles;

  // Clear and rebuild original positions storage
  m_OriginalLocalXZ.clear();

  // Create a disc with concentric rings for better terrain conformance
  // Reduced vertex count for better performance (65 vertices instead of 257)
  const int radialSegments =
      32; // Angular divisions (increased for smoother circle)
  const int concentricRings = 4; // Number of rings from center to edge (was 8)
  const float baseRadius = 0.5f; // Unit radius (will be scaled)

  // Default color (cyan)
  glm::vec3 cyanColor = glm::vec3(0.0f, 1.0f, 1.0f);
  glm::vec3 yellowColor = glm::vec3(1.0f, 1.0f, 0.0f);

  // Center vertex - alpha 0 (invisible center)
  Vertex3D centerVert(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                      glm::vec2(0.0f, 0.0f));
  centerVert.tangent = cyanColor; // Color stored in tangent
  vertices.push_back(centerVert);
  m_OriginalLocalXZ.push_back(glm::vec2(0.0f, 0.0f));

  // Create rings from inner to outer
  for (int ring = 1; ring <= concentricRings; ++ring) {
    float ringRadius = (baseRadius * ring) / concentricRings;
    float radiusPct = (float)ring / concentricRings; // 0 to 1

    // Alpha is 0 for inner 75% of radius, then fades from 0 to 0.75 at the rim
    // This creates a ring shape where only the outer 25% is visible
    float alpha = 0.0f;
    if (radiusPct >= 0.75f) {
      // Linear fade from 0 at 75% to 0.75 at 100%
      alpha = 0.75f * (radiusPct - 0.75f) / 0.25f;
    }

    for (int seg = 0; seg < radialSegments; ++seg) {
      float angle = glm::radians((360.0f / radialSegments) * seg);
      float cosA = cos(angle);
      float sinA = sin(angle);

      float localX = cosA * ringRadius;
      float localZ = sinA * ringRadius;

      // Outermost ring is green, inner rings are cyan
      glm::vec3 vertColor =
          (ring == concentricRings) ? glm::vec3(0.0f, 1.0f, 0.0f) : cyanColor;

      Vertex3D vert(glm::vec3(localX, 0.0f, localZ),
                    glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(alpha, 0.0f));
      vert.tangent = vertColor; // Color stored in tangent
      vertices.push_back(vert);
      m_OriginalLocalXZ.push_back(glm::vec2(localX, localZ));
    }
  }

  // Create triangles
  // Center fan (ring 0 to ring 1)
  for (int seg = 0; seg < radialSegments; ++seg) {
    uint32_t current = 1 + seg;
    uint32_t next = 1 + ((seg + 1) % radialSegments);
    triangles.push_back(Triangle(0, current, next));
  }

  // Ring-to-ring quads
  for (int ring = 1; ring < concentricRings; ++ring) {
    int innerStart = 1 + (ring - 1) * radialSegments;
    int outerStart = 1 + ring * radialSegments;

    for (int seg = 0; seg < radialSegments; ++seg) {
      int innerCurrent = innerStart + seg;
      int innerNext = innerStart + ((seg + 1) % radialSegments);
      int outerCurrent = outerStart + seg;
      int outerNext = outerStart + ((seg + 1) % radialSegments);

      triangles.push_back(Triangle(innerCurrent, outerCurrent, innerNext));
      triangles.push_back(Triangle(outerCurrent, outerNext, innerNext));
    }
  }

  m_Mesh->SetVertices(vertices);
  m_Mesh->SetTriangles(triangles);
  m_Mesh->Finalize(m_Device);
}

void TerrainGizmo::SetPosition(const glm::vec3 &position) {
  // Gizmo position is always at Y=0, mouse pick determines X,Z only
  m_Position = glm::vec3(position.x, 0.0f, position.z);
  m_NeedsTerrainUpdate = true;
}

void TerrainGizmo::SetSize(float size) {
  m_Scale = size;
  m_NeedsTerrainUpdate = true;
}

void TerrainGizmo::UpdateToTerrain(TerrainNode *terrain) {
  if (!terrain || !m_Mesh || !m_Intersections)
    return;

  // m_Scale = 1.0f; // Removed override

  const auto &meshes = terrain->GetMeshes();
  if (meshes.empty())
    return;
  auto terrainMesh = meshes[0];
  if (!terrainMesh)
    return;

  // CRITICAL: Force cache rebuild to use latest terrain vertex data
  // This ensures we're raycasting against the sculpted mesh, not cached old
  // data
  m_Intersections->InvalidateMesh(terrainMesh.get());

  // Get mutable access to gizmo vertices
  std::vector<Vertex3D> &verts =
      const_cast<std::vector<Vertex3D> &>(m_Mesh->GetVertices());

  // Ensure we have original positions for all vertices
  if (m_OriginalLocalXZ.size() != verts.size())
    return;

  glm::mat4 terrainWorld = terrain->GetWorldMatrix();

  for (size_t i = 0; i < verts.size(); ++i) {
    Vertex3D &v = verts[i];
    const glm::vec2 &origXZ = m_OriginalLocalXZ[i];

    // Calculate world X,Z position using ORIGINAL local positions
    // Original positions are in unit space [-0.5, 0.5], scaled and offset
    float worldX = origXZ.x * m_Scale + m_Position.x;
    float worldZ = origXZ.y * m_Scale + m_Position.z;

    // Ray origin: at world X,Z but Y=50 (above terrain)
    glm::vec3 rayOrigin = glm::vec3(worldX, 80.0f, worldZ);
    // Ray direction: straight down (unnormalized, length 50)
    glm::vec3 rayDir = glm::vec3(worldX, -500.0f, worldZ);

    // Raycast against the terrain mesh using GPU-accelerated intersection
    CastResult hit = m_Intersections->CastMesh(terrainWorld, rayOrigin, rayDir,
                                               terrainMesh.get());

    if (hit.Hit && hit.Distance < 500.0f) {
      // Set vertex Y to hit point Y + 0.01 offset
      // Y is NOT scaled in the model matrix (scale is 1.0f for Y)
      // m_Position.y is 0, so world Y = local Y directly
      float hitY = hit.HitPoint.y;
      v.position.y = hitY + 0.01f;

    } else {
      // No hit within range, keep at ground level with small offset
      v.position.y = 0.01f;
    }
  }

  // Upload updated vertices to GPU
  m_Mesh->UpdateVertexBuffer();
  m_NeedsTerrainUpdate = false;
}

CastResult TerrainGizmo::RaycastTerrain(TerrainNode *terrain,
                                        const glm::vec3 &rayOrigin,
                                        const glm::vec3 &rayDir) {
  if (!terrain || !m_Intersections) {
    return CastResult{false};
  }

  const auto &meshes = terrain->GetMeshes();
  if (meshes.empty() || !meshes[0]) {
    return CastResult{false};
  }

  auto terrainMesh = meshes[0];
  glm::mat4 terrainWorld = terrain->GetWorldMatrix();

  // Use a long ray direction for proper intersection distance
  // glm::vec3 longDir = glm::normalize(rayDir) * 1000.0f;

  return m_Intersections->CastMesh(terrainWorld, rayOrigin, rayDir,
                                   terrainMesh.get());
}

void TerrainGizmo::Render(SceneRenderer *renderer, VkCommandBuffer cmd,
                          const glm::mat4 &view, const glm::mat4 &proj) {
  if (!m_Mesh || !m_Mesh->IsFinalized())
    return;

  // Use PLTerrainGizmo pipeline
  auto pipeline = RenderingPipelines::Get().GetPipeline("PLTerrainGizmo");
  if (!pipeline)
    return;

  pipeline->Bind(cmd);

  // Model Matrix with Position AND Scale
  glm::mat4 model = glm::translate(glm::mat4(1.0f), m_Position);
  model = glm::scale(model, glm::vec3(m_Scale, 1.0f, m_Scale));

  // Push Constants: MVP + Color
  struct PushConstants {
    glm::mat4 mvp;
    glm::vec4 color;
  } push;

  push.mvp = proj * view * model;
  push.color = m_Color;

  vkCmdPushConstants(cmd, pipeline->GetPipelineLayout(),
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(PushConstants), &push);

  m_Mesh->Bind(cmd);
  vkCmdDrawIndexed(cmd, static_cast<uint32_t>(m_Mesh->GetIndexCount()), 1, 0, 0,
                   0);
}

} // namespace Quantum
