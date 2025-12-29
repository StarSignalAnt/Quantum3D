#version 450

// Vertex3D layout (simplified - we only need position)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;     // Unused but required for Mesh3D binding
layout(location = 2) in vec2 inUV;         // Unused but required for Mesh3D binding
layout(location = 3) in vec2 inUV2;        // Unused but required for Mesh3D binding
layout(location = 4) in vec3 inTangent;    // Unused but required for Mesh3D binding
layout(location = 5) in vec3 inBitangent;  // Unused but required for Mesh3D binding

// Push constants for MVP and color (no UBO needed!)
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 color;
} pc;

// Output to fragment shader
layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragColor = pc.color;
}
