#version 450

// Inputs from vertex shader
layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragNormal;

// Uniform buffer for color tinting
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec3 viewPos;
    float time;
    vec3 lightPos;
    float clipPlaneDir;
    vec3 lightColor; // Used as Tint Color
    float lightRange;
} ubo;

// Albedo texture sampler (Set 1, Binding 0 for Material)
layout(set = 1, binding = 0) uniform sampler2D albedoTexture;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Sample the albedo texture
    vec4 albedo = texture(albedoTexture, fragUV);
    
    // Multiply by tint color (e.g. Yellow for selection)
    outColor = albedo * vec4(ubo.lightColor, 1.0);
}
