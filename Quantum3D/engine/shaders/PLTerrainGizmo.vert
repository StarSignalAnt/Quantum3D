#version 450

// Vertex3D layout
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;     // Unused
layout(location = 2) in vec2 inUV;         // Used for Alpha (x)
layout(location = 3) in vec2 inUV2;        // Unused
layout(location = 4) in vec3 inTangent;    // Used as per-vertex color (RGB)
layout(location = 5) in vec3 inBitangent;  // Unused

// Push constants for MVP and color
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 color;
} pc;

// Output to fragment shader
layout(location = 0) out float fragAlpha;
layout(location = 1) out vec3 fragColor;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragAlpha = inUV.x;
    fragColor = inTangent;  // Per-vertex color from tangent
}

