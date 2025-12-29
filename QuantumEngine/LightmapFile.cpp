#include "LightmapFile.h"
#include <iostream>

namespace Quantum {

std::string LightmapFile::s_LastError;

bool LightmapFile::Save(const std::string &path,
                        const BakedLightmap &lightmap) {
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) {
    s_LastError = "Failed to open file for writing: " + path;
    return false;
  }

  // Prepare header
  QLightmapHeader header;
  header.width = static_cast<uint32_t>(lightmap.width);
  header.height = static_cast<uint32_t>(lightmap.height);
  header.format = 0; // RGB float
  header.meshNameLength = static_cast<uint32_t>(lightmap.meshName.size());

  // Write header
  file.write(reinterpret_cast<const char *>(&header), sizeof(header));

  // Write mesh name
  if (!lightmap.meshName.empty()) {
    file.write(lightmap.meshName.c_str(), lightmap.meshName.size());
  }

  // Write pixel data (RGB floats)
  if (!lightmap.pixels.empty()) {
    file.write(reinterpret_cast<const char *>(lightmap.pixels.data()),
               lightmap.pixels.size() * sizeof(glm::vec3));
  }

  file.close();

  std::cout << "[LightmapFile] Saved lightmap: " << path << " (" << header.width
            << "x" << header.height << ")" << std::endl;

  return true;
}

bool LightmapFile::Load(const std::string &path, BakedLightmap &lightmap) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    s_LastError = "Failed to open file for reading: " + path;
    return false;
  }

  // Read header
  QLightmapHeader header;
  file.read(reinterpret_cast<char *>(&header), sizeof(header));

  // Validate magic
  if (header.magic[0] != 'Q' || header.magic[1] != 'L' ||
      header.magic[2] != 'M' || header.magic[3] != '1') {
    s_LastError = "Invalid file magic: " + path;
    return false;
  }

  // Validate version
  if (header.version != 1) {
    s_LastError = "Unsupported file version: " + std::to_string(header.version);
    return false;
  }

  // Read mesh name
  if (header.meshNameLength > 0) {
    std::vector<char> nameBuffer(header.meshNameLength);
    file.read(nameBuffer.data(), header.meshNameLength);
    lightmap.meshName = std::string(nameBuffer.begin(), nameBuffer.end());
  } else {
    lightmap.meshName.clear();
  }

  // Read pixel data
  lightmap.width = static_cast<int>(header.width);
  lightmap.height = static_cast<int>(header.height);
  size_t pixelCount = static_cast<size_t>(header.width) * header.height;

  if (pixelCount > 0) {
    lightmap.pixels.resize(pixelCount);
    file.read(reinterpret_cast<char *>(lightmap.pixels.data()),
              pixelCount * sizeof(glm::vec3));
  }

  file.close();

  std::cout << "[LightmapFile] Loaded lightmap: " << path << " ("
            << header.width << "x" << header.height
            << ", mesh: " << lightmap.meshName << ")" << std::endl;

  return true;
}

bool LightmapFile::IsValidFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  QLightmapHeader header;
  file.read(reinterpret_cast<char *>(&header), sizeof(header));

  return (header.magic[0] == 'Q' && header.magic[1] == 'L' &&
          header.magic[2] == 'M' && header.magic[3] == '1' &&
          header.version == 1);
}

const std::string &LightmapFile::GetLastError() { return s_LastError; }

} // namespace Quantum
