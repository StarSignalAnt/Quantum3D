#include "CLLightmapper.h"
#include <exception>
#include <iostream>

namespace Quantum {

CLLightmapper::CLLightmapper() {
  try {
    LoadProgram("engine/CL/lightmapper/lightmapper.cl");
    kernel = cl::Kernel(program, "bakeLightmap");
    m_Initialized = true;
    std::cout << "[CLLightmapper] OpenCL lightmapper initialized successfully"
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[CLLightmapper] OpenCL initialization failed: " << e.what()
              << std::endl;
    m_Initialized = false;
  }
}

bool CLLightmapper::CheckCLError(cl_int err, const char *operation) const {
  if (err != CL_SUCCESS) {
    std::cerr << "[CLLightmapper] OpenCL error in " << operation << ": " << err
              << std::endl;
    return false;
  }
  return true;
}

bool CLLightmapper::BakeLightmap(const std::vector<TexelData> &texels,
                                 const std::vector<LightData> &lights,
                                 const std::vector<float> &triangles,
                                 int numTriangles, bool enableShadows,
                                 std::vector<glm::vec3> &outLighting) {
  if (!m_Initialized) {
    std::cerr << "[CLLightmapper] Not initialized, cannot bake" << std::endl;
    return false;
  }

  if (texels.empty()) {
    std::cerr << "[CLLightmapper] No texels to bake" << std::endl;
    return false;
  }

  size_t numTexels = texels.size();
  size_t numLights = lights.size();

  std::cout << "[CLLightmapper] Baking " << numTexels << " texels with "
            << numLights << " lights and " << numTriangles << " triangles"
            << std::endl;

  cl_int err;

  // Create/update texel buffer
  if (m_CachedTexelCount != numTexels) {
    m_TexelBuffer = cl::Buffer(context, CL_MEM_READ_ONLY,
                               sizeof(TexelData) * numTexels, nullptr, &err);
    if (!CheckCLError(err, "create texel buffer"))
      return false;
    m_CachedTexelCount = numTexels;
  }

  // Create/update light buffer
  if (m_CachedLightCount != numLights && numLights > 0) {
    m_LightBuffer = cl::Buffer(context, CL_MEM_READ_ONLY,
                               sizeof(LightData) * numLights, nullptr, &err);
    if (!CheckCLError(err, "create light buffer"))
      return false;
    m_CachedLightCount = numLights;
  }

  // Create/update triangle buffer
  size_t triFloatCount = triangles.size();
  if (m_CachedTriangleCount != triFloatCount && triFloatCount > 0) {
    m_TriangleBuffer = cl::Buffer(context, CL_MEM_READ_ONLY,
                                  sizeof(float) * triFloatCount, nullptr, &err);
    if (!CheckCLError(err, "create triangle buffer"))
      return false;
    m_CachedTriangleCount = triFloatCount;
  }

  // Create output buffer (3 floats per texel for RGB)
  m_OutputBuffer = cl::Buffer(context, CL_MEM_WRITE_ONLY,
                              sizeof(float) * numTexels * 3, nullptr, &err);
  if (!CheckCLError(err, "create output buffer"))
    return false;

  // Upload data to GPU
  err = queue.enqueueWriteBuffer(m_TexelBuffer, CL_TRUE, 0,
                                 sizeof(TexelData) * numTexels, texels.data());
  if (!CheckCLError(err, "write texel buffer"))
    return false;

  if (numLights > 0) {
    err =
        queue.enqueueWriteBuffer(m_LightBuffer, CL_TRUE, 0,
                                 sizeof(LightData) * numLights, lights.data());
    if (!CheckCLError(err, "write light buffer"))
      return false;
  }

  if (triFloatCount > 0) {
    err = queue.enqueueWriteBuffer(m_TriangleBuffer, CL_TRUE, 0,
                                   sizeof(float) * triFloatCount,
                                   triangles.data());
    if (!CheckCLError(err, "write triangle buffer"))
      return false;
  }

  // Set kernel arguments
  int argIndex = 0;
  err = kernel.setArg(argIndex++, m_TexelBuffer);
  if (!CheckCLError(err, "setArg texels"))
    return false;

  err = kernel.setArg(argIndex++, m_LightBuffer);
  if (!CheckCLError(err, "setArg lights"))
    return false;

  int numLightsInt = static_cast<int>(numLights);
  err = kernel.setArg(argIndex++, numLightsInt);
  if (!CheckCLError(err, "setArg numLights"))
    return false;

  err = kernel.setArg(argIndex++, m_TriangleBuffer);
  if (!CheckCLError(err, "setArg triangles"))
    return false;

  err = kernel.setArg(argIndex++, numTriangles);
  if (!CheckCLError(err, "setArg numTriangles"))
    return false;

  int enableShadowsInt = enableShadows ? 1 : 0;
  err = kernel.setArg(argIndex++, enableShadowsInt);
  if (!CheckCLError(err, "setArg enableShadows"))
    return false;

  err = kernel.setArg(argIndex++, m_OutputBuffer);
  if (!CheckCLError(err, "setArg output"))
    return false;

  // Execute kernel - one work item per texel
  cl::NDRange globalSize(numTexels);
  err = queue.enqueueNDRangeKernel(kernel, cl::NullRange, globalSize,
                                   cl::NullRange);
  if (!CheckCLError(err, "enqueueNDRangeKernel"))
    return false;

  // Read results
  std::vector<float> outputData(numTexels * 3);
  err =
      queue.enqueueReadBuffer(m_OutputBuffer, CL_TRUE, 0,
                              sizeof(float) * numTexels * 3, outputData.data());
  if (!CheckCLError(err, "read output buffer"))
    return false;

  // Convert to glm::vec3
  outLighting.resize(numTexels);
  for (size_t i = 0; i < numTexels; i++) {
    outLighting[i] = glm::vec3(outputData[i * 3 + 0], outputData[i * 3 + 1],
                               outputData[i * 3 + 2]);
  }

  std::cout << "[CLLightmapper] GPU baking complete" << std::endl;
  return true;
}

bool CLLightmapper::BakeIndirect(const std::vector<TexelData> &texels,
                                 const std::vector<LightData> &lights,
                                 const std::vector<float> &sceneTriangles,
                                 int numTriangles, bool enableShadows,
                                 int samples, float intensity,
                                 std::vector<glm::vec3> &outIndirect) {
  if (!m_Initialized)
    return false;

  size_t numTexels = texels.size();
  cl_int err;

  // Ensure buffers are uploaded (reuse logic)
  if (m_CachedTexelCount != numTexels) {
    m_TexelBuffer =
        cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                   sizeof(TexelData) * numTexels, (void *)texels.data(), &err);
    if (!CheckCLError(err, "create texel buffer"))
      return false;
    m_CachedTexelCount = numTexels;
  }

  // Lights
  if (m_CachedLightCount != lights.size() && !lights.empty()) {
    m_LightBuffer = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                               sizeof(LightData) * lights.size(),
                               (void *)lights.data(), &err);
    if (!CheckCLError(err, "create light buffer"))
      return false;
    m_CachedLightCount = lights.size();
  }

  // Triangles
  if (m_CachedTriangleCount != sceneTriangles.size() &&
      !sceneTriangles.empty()) {
    m_TriangleBuffer =
        cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                   sizeof(float) * sceneTriangles.size(),
                   (void *)sceneTriangles.data(), &err);
    if (!CheckCLError(err, "create triangle buffer"))
      return false;
    m_CachedTriangleCount = sceneTriangles.size();
  }

  // Output buffer
  cl::Buffer outBuffer(context, CL_MEM_WRITE_ONLY,
                       sizeof(float) * numTexels * 3, nullptr, &err);
  if (!CheckCLError(err, "create output buffer"))
    return false;

  // Create kernel
  cl::Kernel indirectKernel(program, "bakeIndirect", &err);
  if (!CheckCLError(err, "create bakeIndirect kernel"))
    return false;

  // Set args
  int argIdx = 0;
  err |= indirectKernel.setArg(argIdx++, m_TexelBuffer);
  err |= indirectKernel.setArg(argIdx++, m_LightBuffer);
  err |= indirectKernel.setArg(argIdx++, (int)lights.size());
  err |= indirectKernel.setArg(argIdx++, m_TriangleBuffer);
  err |= indirectKernel.setArg(argIdx++, numTriangles);
  err |= indirectKernel.setArg(argIdx++, enableShadows ? 1 : 0);
  err |= indirectKernel.setArg(argIdx++, samples);
  err |= indirectKernel.setArg(
      argIdx++, 0); // Seed offset (TODO: randomize per bounce if needed)
  err |= indirectKernel.setArg(argIdx++, intensity);
  err |= indirectKernel.setArg(argIdx++, outBuffer);

  if (!CheckCLError(err, "setArg indirect"))
    return false;

  std::cout << "[CLLightmapper] Baking Indirect GI (" << samples
            << " samples)..." << std::endl;

  // Run
  cl::NDRange globalSize(numTexels);
  err = queue.enqueueNDRangeKernel(indirectKernel, cl::NullRange, globalSize,
                                   cl::NullRange);
  if (!CheckCLError(err, "enqueue indirect kernel"))
    return false;

  // Read
  std::vector<float> outputData(numTexels * 3);
  err = queue.enqueueReadBuffer(
      outBuffer, CL_TRUE, 0, sizeof(float) * numTexels * 3, outputData.data());

  if (!CheckCLError(err, "read indirect output"))
    return false;

  // Convert
  outIndirect.resize(numTexels);
  for (size_t i = 0; i < numTexels; i++) {
    outIndirect[i] = glm::vec3(outputData[i * 3 + 0], outputData[i * 3 + 1],
                               outputData[i * 3 + 2]);
  }

  return true;
}

} // namespace Quantum
