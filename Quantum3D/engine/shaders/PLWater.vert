#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

// Uniform buffer - MUST match C++ UniformBufferObject
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float time;      // Time for animation
    vec3 lightPos;
    float clipPlaneDir; // 1.0=clip below Y=0, -1.0=clip above Y=0, 0.0=no clip
    vec3 lightColor;
    float lightRange; 
} ubo;

// Outputs
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec3 fragBitangent;
layout(location = 5) out vec4 fragClipSpace;

void main() {
    // Wave parameters (hardcoded for now, could be pushed via constants)
    float amplitude = 0.5;
    float frequency = 1.0;
    float speed = 1.5;

    // Displacement
    vec3 displacedPos = inPosition;
    
    float freqX = frequency;
    float freqZ = frequency * 0.5;
    float phase = ubo.time * speed;

    float angleX = inPosition.x * freqX + phase;
    float angleZ = inPosition.z * freqZ + phase;

    // Displacement
    float wave = sin(angleX) + cos(angleZ);
    displacedPos.y += wave * amplitude;

    // Derivatives for Normal/Tangent calculation
    // y = A * (sin(angleX) + cos(angleZ))
    // dy/dx = A * (cos(angleX) * freqX)
    // dy/dz = A * (-sin(angleZ) * freqZ)
    float dydx = amplitude * cos(angleX) * freqX;
    float dydz = amplitude * -sin(angleZ) * freqZ;

    // Construct basis vectors
    // Tangent is along X (+ slope y)
    vec3 T = normalize(vec3(1.0, dydx, 0.0));
    // Bitangent is along Z (+ slope y)
    vec3 B = normalize(vec3(0.0, dydz, 1.0));
    // Normal is cross product (upwards)
    vec3 N = normalize(vec3(-dydx, 1.0, -dydz));

    // World Transformation
    // Use the model matrix to transform T, B, N (assuming uniform scale model matrix)
    // For normal matrix, we usually use inverse transpose, but T and B just rotate
    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));
    
    fragNormal = normalize(normalMatrix * N);
    fragTangent = normalize(normalMatrix * T);
    fragBitangent = normalize(normalMatrix * B);
    
    // World position calculation
    vec4 worldPos = ubo.model * vec4(displacedPos, 1.0);
    fragWorldPos = worldPos.xyz;
    fragUV = inUV;

    // Clip space position
    gl_Position = ubo.proj * ubo.view * worldPos;
    fragClipSpace = gl_Position;
}
