#version 450

// Inputs
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;

// Uniforms - MUST match vertex shader UBO layout and C++ struct
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
} ubo;

// Textures (Set 1 - Material Specific)
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;
layout(set = 1, binding = 4) uniform sampler2D reflectionMap;
layout(set = 1, binding = 5) uniform sampler2D refractionMap;

// Clip space input
layout(location = 5) in vec4 fragClipSpace;

// Shadow maps (Set 0 - Global Light Data)
layout(set = 0, binding = 1) uniform samplerCube shadowMap;
layout(set = 0, binding = 2) uniform sampler2D dirShadowMap;

// Output
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Calculate point shadow factor
float calculateShadow(vec3 fragToLight, float currentDepth) {
    float closestDepth = texture(shadowMap, fragToLight).r;
    float shadowFarPlane = ubo.lightRange > 0.0 ? ubo.lightRange : 100.0;
    float normalizedCurrent = currentDepth / shadowFarPlane;
    
    // Adaptive bias: smaller for close objects to prevent shadow disappearing
    float baseBias = 0.0005;  // Very small bias for close objects
    float maxBias = 0.015;
    float bias = mix(baseBias, maxBias, normalizedCurrent);
    
    if (normalizedCurrent > 1.0) return 1.0;
    float shadow = (normalizedCurrent - bias > closestDepth) ? 0.0 : 1.0;
    return shadow;
}

// Calculate directional shadow factor
float calculateDirShadow(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 1.0;
    float closestDepth = texture(dirShadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.005;
    return (currentDepth - bias > closestDepth) ? 0.0 : 1.0;
}

// Fresnel Schlick
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Distribution GGX
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

// Geometry Schlick GGX
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

// Geometry Smith
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

// ... existing code ...

void main() {
    // Water Base Properties
    vec3 baseColor = texture(albedoMap, fragUV).rgb; 
    float metallic  = 0.1;
    float roughness = 0.2; 

    // Animated Normal Mapping
    float speed = 0.05;
    vec2 uv1 = fragUV + vec2(ubo.time * speed, ubo.time * speed * 0.5);
    vec2 uv2 = fragUV + vec2(-ubo.time * speed * 0.7, ubo.time * speed * 0.3);

    // Sample normal map twice
    vec3 n1 = texture(normalMap, uv1).xyz * 2.0 - 1.0;
    vec3 n2 = texture(normalMap, uv2).xyz * 2.0 - 1.0;
    vec3 tangentNormal = normalize(n1 + n2);
    
    // TBN Matrix
    vec3 N_geom = normalize(fragNormal);
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    T = normalize(T - dot(T, N_geom) * N_geom);
    mat3 TBN = mat3(T, B, N_geom);
    vec3 N = normalize(TBN * tangentNormal);
    
    vec3 V = normalize(ubo.viewPos - fragWorldPos);

    // Calculate F0
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, baseColor, metallic);

    // --- Reflection and Refraction Calculation ---
    
    // Calculate NDC coordinates
    vec2 ndc = (fragClipSpace.xy / fragClipSpace.w) / 2.0 + 0.5;
    
    // Distortion strength
    float distortStrength = 0.02; // Reduced strength for realism
    vec2 distortion = tangentNormal.xy * distortStrength;

    // Reflection UV: standard projective mapping (no flip needed if camera is mirrored correctly)
    vec2 reflectUV = ndc + distortion;
    vec2 refractUV = ndc + distortion;

    // Clamp UVs to avoid artifacts
    reflectUV.x = clamp(reflectUV.x, 0.001, 0.999);
    reflectUV.y = clamp(reflectUV.y, 0.001, 0.999);
    refractUV.x = clamp(refractUV.x, 0.001, 0.999);
    refractUV.y = clamp(refractUV.y, 0.001, 0.999);

    vec3 reflectionColor = texture(reflectionMap, reflectUV).rgb; // Removed 3.0x boost
    vec3 refractionColor = texture(refractionMap, refractUV).rgb;

    // Tint refraction with base color (multiplicative)
    // refractionColor = mix(refractionColor, baseColor * refractionColor, 0.5);
    refractionColor *= baseColor; 

    // Simple Fresnel using scalar factor
    float NdotV = max(dot(N, V), 0.0);
    float fresnelFactor = pow(1.0 - NdotV, 3.0);
    fresnelFactor = clamp(fresnelFactor, 0.0, 1.0);

    // Blend using Fresnel
    vec3 waterColor = mix(refractionColor, reflectionColor, fresnelFactor);

    // --- PBR Specular (no shadows) ---
    vec3 L = normalize(ubo.lightPos - fragWorldPos);
    vec3 H = normalize(V + L);
    float distance = length(ubo.lightPos - fragWorldPos);
    
    float rangeFactor = 1.0;
    if (ubo.lightRange > 0.0) {
        rangeFactor = max(0.0, 1.0 - distance / ubo.lightRange);
    }

    float attenuation = 1.0 / (distance * distance + 0.001);
    vec3 radiance = ubo.lightColor * attenuation * rangeFactor;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F_spec = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
    vec3 numerator    = NDF * G * F_spec;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;
    
    float NdotL = max(dot(N, L), 0.0);
    
    // Final color: water color + specular (no shadows)
    vec3 finalColor = waterColor + specular * radiance * NdotL;
    
    // Ambient
    vec3 ambient = vec3(0.05) * baseColor;
    finalColor += ambient;

    outColor = vec4(finalColor, 1.0);
}
