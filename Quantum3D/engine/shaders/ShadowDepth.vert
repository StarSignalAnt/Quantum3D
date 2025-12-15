#version 450

// Vertex attributes (same as PLPBR)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

// Push constants for shadow pass
// Push constants
layout(push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;  // Light view-projection matrix
    mat4 model;             // Model matrix
    vec4 lightPos;          // Light position (xyz = pos, w = farPlane)
} pc;

// Output to fragment shader
layout(location = 0) out vec3 fragWorldPos;

// void main() {
//     // Calculate world position
//     vec4 worldPos = pc.model * vec4(inPosition, 1.0);
//     fragWorldPos = worldPos.xyz;
//     
//     // Transform to light space
//     gl_Position = pc.lightSpaceMatrix * worldPos;
// }

void main() {
    // Calculate world position
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Transform to light space
    gl_Position = pc.lightSpaceMatrix * worldPos;
}
