#include "Intersections.h"
#include "Mesh3D.h"
#include "glm/glm.hpp"
#include <cfloat>
#include <cstring>
#include <iostream>

Intersections::Intersections() {
  LoadProgram("engine/cl/intersects/intersects.cl");
  kernel = cl::Kernel(program, "findClosestIntersection");
}

namespace {
int float_to_int(float f) {
  int i;
  std::memcpy(&i, &f, sizeof(int));
  return i;
}

float int_to_float(int i) {
  float f;
  std::memcpy(&f, &i, sizeof(float));
  return f;
}
} // namespace

bool Intersections::CheckCLError(cl_int err, const char *operation) const {
  if (err != CL_SUCCESS) {
    std::cerr << "OpenCL Error in " << operation << ": " << err << std::endl;
    return false;
  }
  return true;
}

void Intersections::InitializeBuffers() {
  if (!m_CastBuffers.initialized) {
    cl_int err;

    m_CastBuffers.posBuffer =
        cl::Buffer(context, CL_MEM_READ_ONLY, sizeof(glm::vec3), nullptr, &err);
    if (!CheckCLError(err, "InitializeBuffers - posBuffer"))
      return;

    m_CastBuffers.dirBuffer =
        cl::Buffer(context, CL_MEM_READ_ONLY, sizeof(glm::vec3), nullptr, &err);
    if (!CheckCLError(err, "InitializeBuffers - dirBuffer"))
      return;

    m_CastBuffers.resultBuffer =
        cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(int), nullptr, &err);
    if (!CheckCLError(err, "InitializeBuffers - resultBuffer"))
      return;

    m_CastBuffers.hitPointBuffer = cl::Buffer(context, CL_MEM_READ_WRITE,
                                              sizeof(glm::vec3), nullptr, &err);
    if (!CheckCLError(err, "InitializeBuffers - hitPointBuffer"))
      return;

    m_CastBuffers.initialized = true;
  }
}

CastResult Intersections::CastMesh(glm::vec3 pos, glm::vec3 dir,
                                   const Quantum::Mesh3D *mesh) {
  // Thread safety - prevent interference between different cast operations
  std::lock_guard<std::mutex> lock(m_CastMutex);

  // Initialize buffers if needed
  InitializeBuffers();

  if (!mesh) {
    return CastResult{false};
  }

  const auto &triangles = mesh->GetTriangles();
  const auto &vertices = mesh->GetVertices();
  const size_t numTris = triangles.size();

  // Early exit for empty meshes
  if (numTris == 0 || vertices.empty()) {
    return CastResult{false};
  }

  // Check if we have a cached entry for this mesh and if it's still valid
  cl::Buffer *triBuffer = nullptr;
  auto cacheIt = m_MeshCache.find(mesh);
  bool needsRebuild = false;

  uint64_t currentVersion = mesh->GetGeometryVersion();

  if (cacheIt != m_MeshCache.end()) {
    // Check if mesh geometry has changed using version counter
    MeshCacheEntry &entry = cacheIt->second;
    if (entry.geometryVersion != currentVersion) {
      needsRebuild = true;
    }
  } else {
    // No cache entry exists
    needsRebuild = true;
  }

  if (needsRebuild) {
    // Build triangle vertex data (3 vec3 positions per triangle)
    std::vector<glm::vec3> triData;
    triData.reserve(numTris * 3);

    for (const auto &tri : triangles) {
      triData.push_back(vertices[tri.v0].position);
      triData.push_back(vertices[tri.v1].position);
      triData.push_back(vertices[tri.v2].position);
    }

    size_t bufferSize = sizeof(glm::vec3) * triData.size();

    // Create or update cache entry
    MeshCacheEntry &entry = m_MeshCache[mesh];
    entry.triData = std::move(triData);
    entry.geometryVersion = currentVersion;

    // Create or recreate the GPU buffer
    cl_int bufErr;
    entry.triBuffer =
        cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bufferSize,
                   entry.triData.data(), &bufErr);
    if (!CheckCLError(bufErr, "CastMesh - create triangle buffer"))
      return CastResult{false};

    triBuffer = &entry.triBuffer;
  } else {
    // Use cached buffer directly
    triBuffer = &cacheIt->second.triBuffer;
  }

  // Initialize result with FLT_MAX converted to int
  int initialResult = float_to_int(FLT_MAX);
  glm::vec3 initialHitPoint = {0.0f, 0.0f, 0.0f};

  // Write data to buffers with error checking
  cl_int err;
  err = queue.enqueueWriteBuffer(m_CastBuffers.posBuffer, CL_TRUE, 0,
                                 sizeof(glm::vec3), &pos);
  if (!CheckCLError(err, "CastMesh - write posBuffer"))
    return CastResult{false};

  err = queue.enqueueWriteBuffer(m_CastBuffers.dirBuffer, CL_TRUE, 0,
                                 sizeof(glm::vec3), &dir);
  if (!CheckCLError(err, "CastMesh - write dirBuffer"))
    return CastResult{false};

  err = queue.enqueueWriteBuffer(m_CastBuffers.resultBuffer, CL_TRUE, 0,
                                 sizeof(int), &initialResult);
  if (!CheckCLError(err, "CastMesh - write resultBuffer"))
    return CastResult{false};

  err = queue.enqueueWriteBuffer(m_CastBuffers.hitPointBuffer, CL_TRUE, 0,
                                 sizeof(glm::vec3), &initialHitPoint);
  if (!CheckCLError(err, "CastMesh - write hitPointBuffer"))
    return CastResult{false};

  // Set kernel arguments
  err = kernel.setArg(0, m_CastBuffers.posBuffer);
  if (!CheckCLError(err, "CastMesh - setArg 0"))
    return CastResult{false};

  err = kernel.setArg(1, m_CastBuffers.dirBuffer);
  if (!CheckCLError(err, "CastMesh - setArg 1"))
    return CastResult{false};

  err = kernel.setArg(2, m_CastBuffers.resultBuffer);
  if (!CheckCLError(err, "CastMesh - setArg 2"))
    return CastResult{false};

  err = kernel.setArg(3, m_CastBuffers.hitPointBuffer);
  if (!CheckCLError(err, "CastMesh - setArg 3"))
    return CastResult{false};

  err = kernel.setArg(4, *triBuffer);
  if (!CheckCLError(err, "CastMesh - setArg 4"))
    return CastResult{false};

  // Execute kernel
  cl::NDRange globalSize(numTris);
  err = queue.enqueueNDRangeKernel(kernel, cl::NullRange, globalSize,
                                   cl::NullRange);
  if (!CheckCLError(err, "CastMesh - enqueueNDRangeKernel"))
    return CastResult{false};

  // Read results with error checking
  int intResult;
  glm::vec3 hitPoint;
  err = queue.enqueueReadBuffer(m_CastBuffers.resultBuffer, CL_TRUE, 0,
                                sizeof(int), &intResult);
  if (!CheckCLError(err, "CastMesh - read resultBuffer"))
    return CastResult{false};

  err = queue.enqueueReadBuffer(m_CastBuffers.hitPointBuffer, CL_TRUE, 0,
                                sizeof(glm::vec3), &hitPoint);
  if (!CheckCLError(err, "CastMesh - read hitPointBuffer"))
    return CastResult{false};

  // Convert back to float
  float distance = int_to_float(intResult);

  // Process result
  CastResult result;
  result.Hit = false;
  result.Distance = -1.0f;
  result.HitPoint = {0.0f, 0.0f, 0.0f};

  if (distance > -1.0f && distance < 1000.0f) {
    result.Hit = true;
    result.Distance = distance;
    result.HitPoint = hitPoint;
  }

  return result;
}

CastResult Intersections::CastMesh(const glm::mat4 &modelMatrix, glm::vec3 pos,
                                   glm::vec3 dir, const Quantum::Mesh3D *mesh) {
  // Transform ray from world space to local space
  glm::mat4 invModel = glm::inverse(modelMatrix);

  // Transform ray origin to local space
  glm::vec3 localPos = glm::vec3(invModel * glm::vec4(pos, 1.0f));

  // Transform ray direction to local space (no translation for directions)
  // Do NOT normalize - preserve magnitude for distance calculations
  glm::vec3 localDir = glm::vec3(invModel * glm::vec4(dir, 0.0f));

  // Cast in local space
  CastResult result = CastMesh(localPos, localDir, mesh);

  // If hit, transform hit point back to world space
  if (result.Hit) {
    result.HitPoint = glm::vec3(modelMatrix * glm::vec4(result.HitPoint, 1.0f));
  }

  return result;
}

void Intersections::InvalidateMesh(const Quantum::Mesh3D *mesh) {
  std::lock_guard<std::mutex> lock(m_CastMutex);
  m_MeshCache.erase(mesh);
}

void Intersections::ClearCache() {
  std::lock_guard<std::mutex> lock(m_CastMutex);
  m_MeshCache.clear();
}

size_t Intersections::GetOptimalWorkGroupSize(size_t numTris) const {
  // Get device info for optimal work group sizing
  size_t maxWorkGroupSize;
  kernel.getWorkGroupInfo(device, CL_KERNEL_WORK_GROUP_SIZE, &maxWorkGroupSize);

  // Common optimal sizes for AMD GPUs
  const size_t commonSizes[] = {64, 128, 256};

  for (size_t size : commonSizes) {
    if (size <= maxWorkGroupSize && numTris >= size) {
      return size;
    }
  }

  return std::min(maxWorkGroupSize, numTris);
}

size_t Intersections::RoundUpToMultiple(size_t value, size_t multiple) const {
  return ((value + multiple - 1) / multiple) * multiple;
}