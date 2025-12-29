#version 450

// Vertex3D layout matching Mesh3D structure
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec2 inUV2;      // Lightmap UV (unused in simple)
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

// Uniform buffer for transform matrices
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

// Outputs to fragment shader
layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragNormal;

void main() {
    // Transform position to clip space
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // Pass UV coordinates to fragment shader
    fragUV = inUV;
    
    // Transform normal to world space
    fragNormal = mat3(ubo.model) * inNormal;
}
