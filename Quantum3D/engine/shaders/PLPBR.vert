#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

// Uniform buffer (Global frame data) - MUST match C++ UniformBufferObject
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
    float lightType;  // 0 = Point, 1 = Directional, 2 = Spot
    float _pad1, _pad2, _pad3;  // Padding for alignment
} ubo;

// Outputs
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec3 fragBitangent;

// Clip distance for water clipping
out float gl_ClipDistance[1];

void main() {
    // World position
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // UVs
    fragUV = inUV;
    
    // Normal Matrix (to handle non-uniform scaling correctly)
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    
    // Transform TBN to world space
    fragNormal = normalize(normalMatrix * inNormal);
    fragTangent = normalize(normalMatrix * inTangent);
    fragBitangent = normalize(normalMatrix * inBitangent);
    
    // Clip space position
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // Clip plane for water reflection/refraction
    // clipPlaneDir > 0: clip if worldPos.y < 0 (for reflection, keep above water)
    // clipPlaneDir < 0: clip if worldPos.y > 0 (for refraction, keep below water)
    // clipPlaneDir = 0: no clipping (normal rendering)
    float waterHeight = 0.0;
    gl_ClipDistance[0] = (worldPos.y - waterHeight) * ubo.clipPlaneDir;
}
