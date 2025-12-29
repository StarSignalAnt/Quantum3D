#version 450

// Input from vertex shader
layout(location = 0) in float fragAlpha;
layout(location = 1) in vec3 fragColor;

// Push constants (unused but needed for layout compatibility)
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 color;
} pc;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, fragAlpha);
}
