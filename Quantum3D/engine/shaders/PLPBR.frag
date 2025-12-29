#version 450

// Inputs
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec2 fragUV2;    // Lightmap UV coordinates
layout(location = 4) in vec3 fragTangent;
layout(location = 5) in vec3 fragBitangent;

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
    float lightType;  // 0 = Point, 1 = Directional, 2 = Spot
    float _pad1, _pad2, _pad3;  // Padding for alignment
} ubo;

// Textures (all in set 0)
// Textures (Set 1 - Material Specific)
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;

// Shadow cube map (Set 0 - Global Light Data)
layout(set = 0, binding = 1) uniform samplerCube shadowMap;
layout(set = 0, binding = 2) uniform sampler2D dirShadowMap;

// Lightmap texture (Set 1 - Binding 5, same slot as refraction for non-water meshes)
layout(set = 1, binding = 5) uniform sampler2D lightmapTex;

// Output
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

vec3 gridOffsets[20] = vec3[](
   vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1), 
   vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
   vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
   vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
);

// Calculate point shadow factor with optimized PCF
float calculateShadow(vec3 fragToLight, float currentDepth) {
    float shadowFarPlane = ubo.lightRange > 0.0 ? ubo.lightRange : 100.0;
    float normalizedCurrent = currentDepth / shadowFarPlane;
    
    // Adaptive bias: smaller for close objects to prevent shadow disappearing
    float baseBias = 0.0005;  // Very small bias for close objects
    float maxBias = 0.015;
    float bias = mix(baseBias, maxBias, normalizedCurrent);
    
    bias = 0.0001;


    if (normalizedCurrent > 1.0) return 1.0;

    // Optimization: Early exit check with 4 samples
    // diskRadius controls the softness spread
    float viewDistance = length(ubo.viewPos - fragWorldPos);
    float diskRadius = (1.0 + (viewDistance / shadowFarPlane)) / 50.0; 
    
    float earlyShadow = 0.0;
    for(int i = 0; i < 4; ++i) {
        float closestDepth = texture(shadowMap, fragToLight + gridOffsets[i] * diskRadius).r;
        if (normalizedCurrent - bias > closestDepth) {
            earlyShadow += 1.0;
        }
    }
    
    // If all 4 samples are the same, we are either fully in light or fully in shadow
    if (earlyShadow == 0.0) return 1.0;
    if (earlyShadow == 4.0) return 0.0;
    
    // We are on a shadow edge, perform full PCF sampling for smooth gradient
    float shadow = earlyShadow;
    for(int i = 4; i < 20; ++i) {
        float closestDepth = texture(shadowMap, fragToLight + gridOffsets[i] * diskRadius).r;
        if (normalizedCurrent - bias > closestDepth) {
            shadow += 1.0;
        }
    }
    
    return 1.0 - (shadow / 20.0);
}

// Calculate directional shadow factor
// Based on working DX12 reference shader
float calculateDirShadow(vec4 lightSpacePos) {
    // 1. Convert from clip space to NDC [-1, 1]
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // 2. Convert from NDC to texture coordinates [0, 1]
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * 0.5 + 0.5; // No Y-flip needed (Vulkan Y-down is handled in C++ proj)

    // 3. If we are outside the [0,1] range, this pixel is not in the shadow map
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0; // Return full light
    }

    // 4. Get the depth of the current pixel from the light's perspective
    float currentDepth = projCoords.z;

    // 5. Sample the depth stored in the shadow map
    float shadowMapDepth = texture(dirShadowMap, projCoords.xy).r;

    // 6. Use a simple, constant bias
    float bias = 0.005;

    // 7. Perform the comparison (matches DX12 reference)
    if (currentDepth > shadowMapDepth + bias) {
        return 0.0; // The pixel is in shadow
    } else {
        return 1.0; // The pixel is lit
    }
}

// Calculate Normal from Normal Map using TBN matrix
vec3 getNormalFromMap() {
    // Sample normal map and convert from [0,1] to [-1,1]
    vec3 tangentNormal = texture(normalMap, fragUV).xyz * 2.0 - 1.0;

    // Use interpolated TBN vectors directly (already in world space from vertex shader)
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    
    // Build TBN matrix (transforms from tangent space to world space)
    mat3 TBN = mat3(T, B, N);
    
    // Transform tangent-space normal to world space
    return normalize(TBN * tangentNormal);
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

void main() {
    vec3 albedo     = pow(texture(albedoMap, fragUV).rgb, vec3(2.2)); // Linearize
    float metallic  = texture(metallicMap, fragUV).r;
    float roughness = texture(roughnessMap, fragUV).r;

    // Use normal map for per-pixel lighting
    vec3 N = normalize(fragNormal);
    
    // Normal mapping
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    T = normalize(T - dot(T, N) * N);  // Gram-Schmidt re-orthogonalization
    mat3 TBN = mat3(T, B, N);
    vec3 tangentNormal = texture(normalMap, fragUV).xyz * 2.0 - 1.0;
    vec3 N_pixel = normalize(TBN * tangentNormal);
    N = N_pixel;
    
    vec3 V = normalize(ubo.viewPos - fragWorldPos);

    // F0 for dielectrics is 0.04, for metals it matches albedo
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // Reflectance equation
    vec3 Lo = vec3(0.0);

    // Calculate light direction based on light type
    // lightType: 0 = Point, 1 = Directional, 2 = Spot
    vec3 L;
    float distance;
    float attenuation;
    float rangeFactor = 1.0;
    
    if (ubo.lightType < 0.5) {
        // Point Light (type 0): lightPos is a position, calculate direction from fragment
        L = normalize(ubo.lightPos - fragWorldPos);
        distance = length(ubo.lightPos - fragWorldPos);
        if (ubo.lightRange > 0.0) {
            rangeFactor = max(0.0, 1.0 - distance / ubo.lightRange);
        }
        attenuation = 1.0 / (distance * distance + 0.001);
    } else {
        // Directional Light (type 1): lightPos IS the direction the light is POINTING
        // We NEGATE because L should be the direction FROM fragment TO light source
        L = -normalize(ubo.lightPos);
        distance = 1.0;
        attenuation = 1.0; // No distance falloff for directional lights
    }
    
    // Safe half-vector calculation
    vec3 H_raw = V + L;
    float H_len = length(H_raw);
    vec3 H = H_len > 0.0001 ? H_raw / H_len : N;

    vec3 radiance = ubo.lightColor * attenuation * rangeFactor;

    // Calculate shadow
    float shadow = 1.0;
    if (ubo.lightType < 0.5) {
        // Point Light Shadow
        vec3 fragToLight = fragWorldPos - ubo.lightPos;
        fragToLight.x = -fragToLight.x; 
        shadow = calculateShadow(fragToLight, distance);
    } else {
        // Directional Light Shadow
        vec4 fragPosLightSpace = ubo.lightSpaceMatrix * vec4(fragWorldPos, 1.0);
        shadow = calculateDirShadow(fragPosLightSpace);
    }

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
    F = clamp(F, vec3(0.0), vec3(1.0));
       
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;
    specular = clamp(specular, vec3(0.0), vec3(10.0));
        
    // Diffuse
    float NdotL = max(dot(N, L), 0.0);        
    vec3 diffuse = albedo / PI;
    diffuse *= (1.0 - metallic);
    
    // Combine with shadow
    Lo += (diffuse + specular) * radiance * NdotL * shadow; 
    
    // Ambient (small amount so fully shadowed areas aren't completely black)
    vec3 ambient = vec3(0.03) * albedo;
    
    // Check if mesh has lightmap (UV2 non-zero)
    bool hasLightmap = (fragUV2.x > 0.0 || fragUV2.y > 0.0);
    
    if (hasLightmap) {
        // Baked lighting: albedo * lightmap (no real-time lighting)
        vec3 lightmapColor = texture(lightmapTex, fragUV2).rgb;
        vec3 color = albedo * lightmapColor + ambient;
        outColor = vec4(color, 1.0);
        return;
    }
    
    // Real-time lighting path (no lightmap)
    vec3 color = ambient + Lo;

    outColor = vec4(color, 1.0);
}
