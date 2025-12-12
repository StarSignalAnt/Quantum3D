#version 450

// Inputs
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;

// Uniforms (Must match Vertex Shader UBO layout)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float padding;
    vec3 lightPos;
    float padding2;
    vec3 lightColor;
    float lightRange; // 0 = infinite range, otherwise max light distance
} ubo;

// Textures
// Binding 1: Albedo
// Binding 2: Normal
// Binding 3: Metallic
// Binding 4: Roughness
layout(set = 0, binding = 1) uniform sampler2D albedoMap;
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D metallicMap;
layout(set = 0, binding = 4) uniform sampler2D roughnessMap;

// Output
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Calculate Normal from Normal Map using TBN matrix
vec3 getNormalFromMap() {
    // Sample normal map and convert from [0,1] to [-1,1]
    vec3 tangentNormal = texture(normalMap, fragUV).xyz * 2.0 - 1.0;
    
    // TEMPORARY FIX: Green flip removed (didn't fix)
    // tangentNormal.y = -tangentNormal.y;

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
    float ao = 1.0; // AO Map Removed
    vec3 emissive = vec3(0.0); // Emissive Map Removed

    // Use normal map for per-pixel lighting (TBN-based normal mapping)
    // Gram-Schmidt process to re-orthogonalize TBN
    vec3 N_geom = normalize(fragNormal);
    vec3 T_geom = normalize(fragTangent);
    // Re-orthogonalize T with respect to N
    T_geom = normalize(T_geom - dot(T_geom, N_geom) * N_geom);
    // Retrieve B (we can use the one passed from vertex shader, but let's recompute to be safe/orthogonal)
    // Actually, let's use the bitangent passed in but orthogonalize it too, or just cross product?
    // Using cross product assumes a specific handedness. The vertex shader passes bitangent.
    // Let's use the passed bitangent but orthogonalize it against N and T.
    vec3 B_geom = normalize(fragBitangent);
    // Strict Gram-Schmidt on B might be overkill if we just want a valid frame, 
    // but ensures orthogonality.
    // Simpler efficient way: B = cross(N, T) * sign. But we don't have sign easily.
    // Let's just trust T and N are good now, and re-orthogonalize B against them?
    // Or just use the original function with the improved vectors?
    
    // Let's stick to the simplest improvement:
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    
    // Gram-Schmidt re-orthogonalization
    T = normalize(T - dot(T, N) * N);
    // Re-compute B to ensure it's perpendicular to both N and T
    // Note: This assumes specific winding order. If normal map looks flipped, check this.
    // For now, let's just use the passed bitangent, but orthogonalized.
    // B = normalize(B - dot(B, N) * N - dot(B, T) * T); // Full re-orthogonalization
    
    // Mat3 for TBN
    mat3 TBN = mat3(T, B, N);

    // Sample normal map
    vec3 tangentNormal = texture(normalMap, fragUV).xyz * 2.0 - 1.0;
    vec3 N_pixel = normalize(TBN * tangentNormal);
    N = N_pixel; // Use the pixel normal for lighting
    vec3 V = normalize(ubo.viewPos - fragWorldPos);

    // F0 for dielectrics is 0.04, for metals it matches albedo
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // Reflectance equation
    vec3 Lo = vec3(0.0);

    // Single point light
    vec3 L = normalize(ubo.lightPos - fragWorldPos);
    
    // Safe half-vector calculation - avoid NaN when V and L are opposite
    vec3 H_raw = V + L;
    float H_len = length(H_raw);
    vec3 H = H_len > 0.0001 ? H_raw / H_len : N;  // Fallback to N if V and L are opposite
    
    // Calculate distance from pixel to light
    float xd = ubo.lightPos.x - fragWorldPos.x;
    float yd = ubo.lightPos.y - fragWorldPos.y;
    float zd = ubo.lightPos.z - fragWorldPos.z;
    float distance = sqrt(xd*xd + yd*yd + zd*zd);
    
    // Range-based linear falloff:
    // - At light position (distance=0): rangeFactor = 1.0 (full intensity)
    // - At halfway (distance=range/2): rangeFactor = 0.5 (half intensity)
    // - At range or beyond: rangeFactor = 0.0 (no light)
    // When lightRange is 0, it means infinite range (no falloff)
    float rangeFactor = 1.0;
    if (ubo.lightRange > 0.0) {
        rangeFactor = max(0.0, 1.0 - distance / ubo.lightRange);
    }
    float lc = rangeFactor;
    
    float attenuation = 1.0 / (distance * distance + 0.001);  // Avoid div by zero
    vec3 radiance     = ubo.lightColor * attenuation * rangeFactor;  // Apply range falloff

    // Cook-Torrance BRDF for specular only
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
    F = clamp(F, vec3(0.0), vec3(1.0));
       
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;
    specular = clamp(specular, vec3(0.0), vec3(10.0));  // Clamp to prevent crazy values
        
    // Simplified: Direct diffuse without Fresnel energy conservation
    // This avoids the dark spot issue while still looking good
    float NdotL = max(dot(N, L), 0.0);        
    vec3 diffuse = albedo / PI;
    
    // Blend based on metallic - metals have no diffuse
    diffuse *= (1.0 - metallic);
    
    // Combine diffuse and specular
    Lo += (diffuse + specular) * radiance * NdotL; 

    // No ambient lighting - will be added later as GI
    vec3 ambient = vec3(0.0);
    
    vec3 color = Lo + ambient + emissive; // Add emissive term

    // HDR tonemapping
   // color = color / (color + vec3(1.0));
    // Gamma correction
   // color = pow(color, vec3(1.0/2.2)); 


    outColor = vec4(color*lc, 1.0);
}
