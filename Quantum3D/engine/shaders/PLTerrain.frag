#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTiledUV;
layout(location = 3) in vec2 fragLayerUV;
layout(location = 4) in vec3 fragTangent;
layout(location = 5) in vec3 fragBitangent;
layout(location = 6) in vec4 fragLightSpacePos;

// Uniforms - MUST match PLPBR.frag UBO layout exactly
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
    float lightType;  // 0 = Point, 1 = Directional, 2 = Spot
    float _pad1, _pad2, _pad3;  // Padding for alignment
} ubo;

// Layer textures (Set 1)
// 4 layers - each layer has color, normal, specular, and layer map
layout(set = 1, binding = 0) uniform sampler2D layer0Color;
layout(set = 1, binding = 1) uniform sampler2D layer0Normal;
layout(set = 1, binding = 2) uniform sampler2D layer0Specular;
layout(set = 1, binding = 3) uniform sampler2D layer0Map;

layout(set = 1, binding = 4) uniform sampler2D layer1Color;
layout(set = 1, binding = 5) uniform sampler2D layer1Normal;
layout(set = 1, binding = 6) uniform sampler2D layer1Specular;
layout(set = 1, binding = 7) uniform sampler2D layer1Map;

layout(set = 1, binding = 8) uniform sampler2D layer2Color;
layout(set = 1, binding = 9) uniform sampler2D layer2Normal;
layout(set = 1, binding = 10) uniform sampler2D layer2Specular;
layout(set = 1, binding = 11) uniform sampler2D layer2Map;

layout(set = 1, binding = 12) uniform sampler2D layer3Color;
layout(set = 1, binding = 13) uniform sampler2D layer3Normal;
layout(set = 1, binding = 14) uniform sampler2D layer3Specular;
layout(set = 1, binding = 15) uniform sampler2D layer3Map;

// Shadow maps
layout(set = 0, binding = 1) uniform samplerCube shadowMap;
layout(set = 0, binding = 2) uniform sampler2D dirShadowMap;

// Output
layout(location = 0) out vec4 outColor;

// Calculate point shadow factor
vec3 gridOffsets[20] = vec3[](
   vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1), 
   vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
   vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
   vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
);

float calculatePointShadow(vec3 fragToLight, float currentDepth) {
    float shadowFarPlane = ubo.lightRange > 0.0 ? ubo.lightRange : 100.0;
    float normalizedCurrent = currentDepth / shadowFarPlane;
    float bias = 0.0001;

    if (normalizedCurrent > 1.0) return 1.0;

    float viewDistance = length(ubo.viewPos - fragWorldPos);
    float diskRadius = (1.0 + (viewDistance / shadowFarPlane)) / 50.0; 
    
    float shadow = 0.0;
    for(int i = 0; i < 20; ++i) {
        float closestDepth = texture(shadowMap, fragToLight + gridOffsets[i] * diskRadius).r;
        if (normalizedCurrent - bias > closestDepth) {
            shadow += 1.0;
        }
    }
    
    return 1.0 - (shadow / 20.0);
}

// Calculate directional shadow factor
float calculateDirShadow(vec4 lightSpacePos) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    float currentDepth = projCoords.z;
    float shadowMapDepth = texture(dirShadowMap, projCoords.xy).r;
    float bias = 0.005;

    if (currentDepth > shadowMapDepth + bias) {
        return 0.0;
    }
    return 1.0;
}

void main() {
    // Sample all layers and blend by their layer maps
    float totalWeight = 0.0;
    vec3 blendedColor = vec3(0.0);
    vec3 blendedNormal = vec3(0.0);
    float blendedSpecular = 0.0;
    
    // Layer 0
    float w0 = texture(layer0Map, fragLayerUV).r;
    blendedColor += texture(layer0Color, fragTiledUV).rgb * w0;
    blendedNormal += (texture(layer0Normal, fragTiledUV).rgb * 2.0 - 1.0) * w0;
    blendedSpecular += texture(layer0Specular, fragTiledUV).r * w0;
    totalWeight += w0;
    
    // Layer 1
    float w1 = texture(layer1Map, fragLayerUV).r;
    blendedColor += texture(layer1Color, fragTiledUV).rgb * w1;
    blendedNormal += (texture(layer1Normal, fragTiledUV).rgb * 2.0 - 1.0) * w1;
    blendedSpecular += texture(layer1Specular, fragTiledUV).r * w1;
    totalWeight += w1;
    
    // Layer 2
    float w2 = texture(layer2Map, fragLayerUV).r;
    blendedColor += texture(layer2Color, fragTiledUV).rgb * w2;
    blendedNormal += (texture(layer2Normal, fragTiledUV).rgb * 2.0 - 1.0) * w2;
    blendedSpecular += texture(layer2Specular, fragTiledUV).r * w2;
    totalWeight += w2;
    
    // Layer 3
    float w3 = texture(layer3Map, fragLayerUV).r;
    blendedColor += texture(layer3Color, fragTiledUV).rgb * w3;
    blendedNormal += (texture(layer3Normal, fragTiledUV).rgb * 2.0 - 1.0) * w3;
    blendedSpecular += texture(layer3Specular, fragTiledUV).r * w3;
    totalWeight += w3;
    
    // Normalize by total weight
    if (totalWeight > 0.001) {
        blendedColor /= totalWeight;
        blendedNormal /= totalWeight;
        blendedSpecular /= totalWeight;
    } else {
        blendedColor = texture(layer0Color, fragTiledUV).rgb;
        blendedNormal = texture(layer0Normal, fragTiledUV).rgb * 2.0 - 1.0;
        blendedSpecular = texture(layer0Specular, fragTiledUV).r;
    }
    
    // Transform normal to world space using TBN matrix
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent - dot(fragTangent, N) * N);
    vec3 B = normalize(fragBitangent);
    mat3 TBN = mat3(T, B, N);
    vec3 worldNormal = normalize(TBN * normalize(blendedNormal));
    
    // View direction
    vec3 V = normalize(ubo.viewPos - fragWorldPos);
    
    // Calculate light direction based on light type
    vec3 L;
    float distance;
    float attenuation;
    float rangeFactor = 1.0;
    
    if (ubo.lightType < 0.5) {
        // Point Light: lightPos is a position
        L = normalize(ubo.lightPos - fragWorldPos);
        distance = length(ubo.lightPos - fragWorldPos);
        if (ubo.lightRange > 0.0) {
            rangeFactor = max(0.0, 1.0 - distance / ubo.lightRange);
        }
        attenuation = 1.0 / (distance * distance + 0.001);
    } else {
        // Directional Light: lightPos IS the direction (negate for L)
        L = -normalize(ubo.lightPos);
        distance = 1.0;
        attenuation = 1.0;
    }
    
    vec3 H = normalize(V + L);
    
    // Diffuse
    float NdotL = max(dot(worldNormal, L), 0.0);
    vec3 diffuse = blendedColor * NdotL;
    
    // Specular (Blinn-Phong)
    float NdotH = max(dot(worldNormal, H), 0.0);
    float specPower = 32.0;
    vec3 specular = vec3(blendedSpecular) * pow(NdotH, specPower);
    
    // Shadow
    float shadow = 1.0;
    if (ubo.lightType < 0.5) {
        // Point Light Shadow
        vec3 fragToLight = fragWorldPos - ubo.lightPos;
        fragToLight.x = -fragToLight.x;
        shadow = calculatePointShadow(fragToLight, distance);
    } else {
        // Directional Light Shadow
        vec4 fragPosLightSpace = ubo.lightSpaceMatrix * vec4(fragWorldPos, 1.0);
        shadow = calculateDirShadow(fragPosLightSpace);
    }
    
    // Radiance
    vec3 radiance = ubo.lightColor * attenuation * rangeFactor;
    
    // Ambient
    vec3 ambient = blendedColor * 0.03;
    
    // Final color
    vec3 finalColor = ambient + (diffuse + specular) * radiance * shadow;
    
    outColor = vec4(finalColor, 1.0);
}
