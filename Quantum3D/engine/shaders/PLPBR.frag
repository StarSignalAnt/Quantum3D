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
    vec3 viewPos;
    float padding;
    vec3 lightPos;
    float padding2;
    vec3 lightColor;
    float lightRange; 
} ubo;

// Textures (all in set 0)
layout(set = 0, binding = 1) uniform sampler2D albedoMap;
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D metallicMap;
layout(set = 0, binding = 4) uniform sampler2D roughnessMap;

// Shadow cube map
layout(set = 0, binding = 5) uniform samplerCube shadowMap;

// Output
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Calculate shadow factor (0 = fully shadowed, 1 = fully lit)
float calculateShadow(vec3 fragToLight, float currentDepth) {
    // Sample the cube map using the direction from light to fragment
    float closestDepth = texture(shadowMap, fragToLight).r;
    
    // Get the far plane used for shadow rendering
    float shadowFarPlane = ubo.lightRange > 0.0 ? ubo.lightRange : 100.0;
    
    // Normalize current depth to match shadow map range (0-1)
    float normalizedCurrent = currentDepth / shadowFarPlane;
    
    // Simple fixed bias
    float bias = 0.01;
    
    // If we're past the far plane, no shadow
    if (normalizedCurrent > 1.0) {
        return 1.0;
    }
    
    // Shadow: if current fragment is further than stored closest, we're in shadow
    // closestDepth stores the distance to the nearest occluder
    // If normalizedCurrent > closestDepth, something is between us and the light
    float shadow = (normalizedCurrent - bias > closestDepth) ? 0.0 : 1.0;
    
    return shadow;
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
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    
    // Gram-Schmidt re-orthogonalization
    T = normalize(T - dot(T, N) * N);
    
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

    // Single point light
    vec3 L = normalize(ubo.lightPos - fragWorldPos);
    
    // Safe half-vector calculation
    vec3 H_raw = V + L;
    float H_len = length(H_raw);
    vec3 H = H_len > 0.0001 ? H_raw / H_len : N;
    
    // Calculate distance from pixel to light
    float distance = length(ubo.lightPos - fragWorldPos);
    
    // Range-based linear falloff
    float rangeFactor = 1.0;
    if (ubo.lightRange > 0.0) {
        rangeFactor = max(0.0, 1.0 - distance / ubo.lightRange);
    }
    
    float attenuation = 1.0 / (distance * distance + 0.001);
    vec3 radiance = ubo.lightColor * attenuation * rangeFactor;

    // Calculate shadow
    vec3 fragToLight = fragWorldPos - ubo.lightPos;
    fragToLight.x = - fragToLight.x;
    float shadow = calculateShadow(fragToLight, distance);

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
    
    vec3 color = Lo + ambient;

    

    outColor = vec4(color, 1.0);
}
