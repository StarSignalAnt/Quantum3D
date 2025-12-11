#version 450

// Inputs from vertex shader
layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragNormal;

// Albedo texture sampler (binding 1 in descriptor set)
layout(set = 0, binding = 1) uniform sampler2D albedoTexture;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Sample the albedo texture - no lighting, just the texture color
    vec4 albedo = texture(albedoTexture, fragUV);
    
    // Output the texture color directly
    outColor = albedo;
}
