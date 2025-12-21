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
    float time;      // Changed
    vec3 lightPos;
    float padding2;
    vec3 lightColor;
    float lightRange; 
} ubo;

// Textures (Set 1 - Material Specific)
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;

// Shadow cube map (Set 0 - Global Light Data)
layout(set = 0, binding = 1) uniform samplerCube shadowMap;

// Output
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Calculate shadow factor (0 = fully shadowed, 1 = fully lit)
float calculateShadow(vec3 fragToLight, float currentDepth) {
    float closestDepth = texture(shadowMap, fragToLight).r;
    float shadowFarPlane = ubo.lightRange > 0.0 ? ubo.lightRange : 100.0;
    float normalizedCurrent = currentDepth / shadowFarPlane;
    float bias = 0.01;
    
    if (normalizedCurrent > 1.0) {
        return 1.0;
    }
    
    float shadow = (normalizedCurrent - bias > closestDepth) ? 0.0 : 1.0;
    return shadow;
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
    float NdotL = max(dot(N, L), 0.0); // Reverted to max
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main() {
    // Water Base Properties
    vec3 baseColor = texture(albedoMap, fragUV).rgb; // Uses the blue texture we set
    float metallic  = 0.1;
    float roughness = 0.2; // Shiny

    // Animated Normal Mapping
    // Scrolling UVs
    float speed = 0.05;
    vec2 uv1 = fragUV + vec2(ubo.time * speed, ubo.time * speed * 0.5);
    vec2 uv2 = fragUV + vec2(-ubo.time * speed * 0.7, ubo.time * speed * 0.3);

    // Sample normal map twice
    vec3 n1 = texture(normalMap, uv1).xyz * 2.0 - 1.0;
    vec3 n2 = texture(normalMap, uv2).xyz * 2.0 - 1.0;
    
    // Blend normals
    vec3 tangentNormal = normalize(n1 + n2);
    
    // TBN Matrix
    vec3 N_geom = normalize(fragNormal);
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    
    // Gram-Schmidt
    T = normalize(T - dot(T, N_geom) * N_geom);
    mat3 TBN = mat3(T, B, N_geom);
    
    // Final World Normal
    vec3 N = normalize(TBN * tangentNormal);
    
    // View Vector
    vec3 V = normalize(ubo.viewPos - fragWorldPos);

    // F0 for water (dielectric)
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, baseColor, metallic);

    // Lighting
    vec3 Lo = vec3(0.0);
    vec3 L = normalize(ubo.lightPos - fragWorldPos);
    vec3 H = normalize(V + L);
    float distance = length(ubo.lightPos - fragWorldPos);
    
    float rangeFactor = 1.0;
    if (ubo.lightRange > 0.0) {
        rangeFactor = max(0.0, 1.0 - distance / ubo.lightRange);
    }
    
    float attenuation = 1.0 / (distance * distance + 0.001);
    vec3 radiance = ubo.lightColor * attenuation * rangeFactor;
    
    // Shadow
    vec3 fragToLight = fragWorldPos - ubo.lightPos;
    // Fix incorrect negative X flip which was likely a hack
    // fragToLight.x = -fragToLight.x; 

    // Calculate shadow with increased bias for wave displacement
    float closestDepth = texture(shadowMap, fragToLight).r;
    float shadowFarPlane = ubo.lightRange > 0.0 ? ubo.lightRange : 100.0;
    float normalizedCurrent = distance / shadowFarPlane;
    
    // Much larger bias for water to prevent self-shadowing from flat-plane shadow map
    float bias = 0.05; 
    
    float shadow = 0.0;
    if (normalizedCurrent > 1.0) {
        shadow = 1.0;
    } else {
        shadow = (normalizedCurrent - bias > closestDepth) ? 0.0 : 1.0;
    }

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;
        
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallic);	  
    
    // Reverted to single-sided lighting
    float NdotL = max(dot(N, L), 0.0);
    
    Lo += (kD * baseColor / PI + specular) * radiance * NdotL * shadow; 
    
    // Ambient
    vec3 ambient = vec3(0.05) * baseColor;
    
    vec3 color = ambient + Lo;

    // HDR / Gamma correction (if needed, usually done in post-process but doing consistent with PLPBR)
    // color = color / (color + vec3(1.0));
    // color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, 0.8); // Slight transparency? For now opaque as blend is disabled in PBR pipeline by default
}
