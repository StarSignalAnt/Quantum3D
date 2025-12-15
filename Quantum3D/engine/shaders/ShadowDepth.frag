#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragWorldPos;

// Push constants
// Push constants
layout(push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;  // Light view-projection matrix
    mat4 model;             // Model matrix
    vec4 lightPos;          // Light position (xyz = pos, w = farPlane)
} pc;

void main() {
    // Calculate distance from fragment to light
    float lightDistance = length(fragWorldPos - pc.lightPos.xyz);
    
    // Normalize distance by far plane (0 = at light, 1 = at far plane)
    // This gives us linear depth for point light shadows
    float farPlane = pc.lightPos.w;
    gl_FragDepth = lightDistance / farPlane;
}
