#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

// Uniform buffer - MUST match C++ UniformBufferObject
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec3 viewPos;
    float time;
    vec3 lightPos;
    float clipPlaneDir;
    vec3 lightColor;
    float lightRange;
} ubo;

// Outputs
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTiledUV;    // For color/normal/specular (tiled)
layout(location = 3) out vec2 fragLayerUV;    // For layer maps (0-1 span)
layout(location = 4) out vec3 fragTangent;
layout(location = 5) out vec3 fragBitangent;
layout(location = 6) out vec4 fragLightSpacePos;

void main() {
    // World position
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Normal matrix for correct normal transformation
    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    fragTangent = normalize(normalMatrix * inTangent);
    fragBitangent = normalize(normalMatrix * inBitangent);
    
    // UV coordinates
    // inUV is expected to be 0-1 across the terrain (layer UV)
    fragLayerUV = inUV;
    
    // Tiled UV for textures - scale for visual tiling
    float tilingFactor = 16.0; // Repeat textures across terrain
    fragTiledUV = inUV * tilingFactor;
    
    // Light space position for shadows
    fragLightSpacePos = ubo.lightSpaceMatrix * worldPos;
    
    // Clip space position
    gl_Position = ubo.proj * ubo.view * worldPos;
}
