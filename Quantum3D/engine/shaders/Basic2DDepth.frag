#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

// Use sampler2D for depth textures in visualization mode
// The depth value comes in the R channel when sampling D32_SFLOAT
layout(binding = 0) uniform sampler2D depthSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample depth value - it comes as a single float in the R channel
    float depth = texture(depthSampler, inUV).r;
    
    // Visualize depth as grayscale
    // Depth 0.0 = very close (white), 1.0 = far (black)
    // Invert to make near objects bright
    float visualValue = 1.0 - depth;
    
    // Apply tint color
    outColor = vec4(visualValue, visualValue, visualValue, 1.0) * inColor;
}
