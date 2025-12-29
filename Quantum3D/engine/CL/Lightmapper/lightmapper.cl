// Lightmapper OpenCL Kernel
// GPU-accelerated lightmap baking with shadow ray tracing
// Author: Quantum3D Engine

#define EPSILON 1e-4f
#define SHADOW_BIAS 0.05f
#define MAX_DISTANCE 10000.0f
#define PI 3.14159265359f

// ============================================================================
// Data Structures (Aligns with C++ 16-byte alignment)
// ============================================================================

typedef struct {
    float4 worldPos; // w=ignored
    float4 normal;   // w=ignored
    int valid;
    int _padding[3]; // Align to 48 bytes
} Texel;

typedef struct {
    float4 positionAndRange; // xyz=pos, w=range
    float4 colorAndType;     // xyz=color, w=type (0=Point, 1=Directional)
    float4 direction;        // xyz=dir, w=ignored
} Light;

// ============================================================================
// Random Number Generation (Wang Hash)
// ============================================================================

uint wang_hash(uint seed) {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

float randomFloat(uint* seed) {
    *seed = wang_hash(*seed);
    return (float)(*seed) / (float)(0xFFFFFFFF);
}

// ============================================================================
// Sampling
// ============================================================================

// Cosine weighted hemisphere sampling
float3 sampleHemisphere(float3 normal, uint* seed) {
    float r1 = randomFloat(seed);
    float r2 = randomFloat(seed);
    
    float theta = 2.0f * PI * r1;
    float phi = acos(sqrt(r2));
    
    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    
    // Transform to world space
    float3 up = fabs(normal.z) < 0.999f ? (float3)(0.0f, 0.0f, 1.0f) : (float3)(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    return tangent * x + bitangent * y + normal * z;
}

// ============================================================================
// Ray-Triangle Intersection
// ============================================================================

float rayTriangleIntersect(
    float3 rayOrigin, 
    float3 rayDir,
    float3 v0, 
    float3 v1, 
    float3 v2
) {
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 h = cross(rayDir, edge2);
    float a = dot(edge1, h);
    
    if (fabs(a) < EPSILON) return -1.0f;
    
    float f = 1.0f / a;
    float3 s = rayOrigin - v0;
    float u = f * dot(s, h);
    
    if (u < 0.0f || u > 1.0f) return -1.0f;
    
    float3 q = cross(s, edge1);
    float v = f * dot(rayDir, q);
    
    if (v < 0.0f || u + v > 1.0f) return -1.0f;
    
    float t = f * dot(edge2, q);
    
    if (t > EPSILON) return t;
    
    return -1.0f;
}

// ============================================================================
// Ray Tracing
// ============================================================================

// Returns 1.0 if visible, 0.0 if occluded
float traceShadowRay(
    float3 origin,
    float3 rayDir,
    float maxDist,
    __global float* triangles,
    int numTriangles
) {
    for (int i = 0; i < numTriangles; i++) {
        int idx = i * 9;
        float3 v0 = (float3)(triangles[idx + 0], triangles[idx + 1], triangles[idx + 2]);
        float3 v1 = (float3)(triangles[idx + 3], triangles[idx + 4], triangles[idx + 5]);
        float3 v2 = (float3)(triangles[idx + 6], triangles[idx + 7], triangles[idx + 8]);
        
        float t = rayTriangleIntersect(origin, rayDir, v0, v1, v2);
        
        if (t > 0.0f && t < maxDist - EPSILON) {
            return 0.0f;
        }
    }
    return 1.0f;
}

// Returns distance to closest intersection, or -1.0 if none
// Outputs hit index
float findClosestIntersection(
    float3 origin,
    float3 rayDir,
    __global float* triangles,
    int numTriangles,
    int* outTriIndex
) {
    float closestT = MAX_DISTANCE;
    int closestIdx = -1;
    
    for (int i = 0; i < numTriangles; i++) {
        int idx = i * 9;
        float3 v0 = (float3)(triangles[idx + 0], triangles[idx + 1], triangles[idx + 2]);
        float3 v1 = (float3)(triangles[idx + 3], triangles[idx + 4], triangles[idx + 5]);
        float3 v2 = (float3)(triangles[idx + 6], triangles[idx + 7], triangles[idx + 8]);
        
        float t = rayTriangleIntersect(origin, rayDir, v0, v1, v2);
        
        if (t > 0.0f && t < closestT) {
            closestT = t;
            closestIdx = i;
        }
    }
    
    if (closestIdx != -1) {
        *outTriIndex = closestIdx;
        return closestT;
    }
    return -1.0f;
}

// ============================================================================
// Lighting Logic
// ============================================================================

float3 computeDirectLighting(
    float3 worldPos,
    float3 normal,
    __global Light* lights,
    int numLights,
    __global float* triangles,
    int numTriangles,
    int enableShadows
) {
    float3 totalLight = (float3)(0.0f);
    
    for (int l = 0; l < numLights; l++) {
        Light light = lights[l];
        float3 lightPos = light.positionAndRange.xyz;
        float lightRange = light.positionAndRange.w;
        float3 lightColor = light.colorAndType.xyz;
        int lightType = (int)light.colorAndType.w;
        float3 lightDirection = normalize(light.direction.xyz);
        
        float3 L = (float3)(0.0f);
        float distance = MAX_DISTANCE;
        float attenuation = 1.0f;
        
        if (lightType == 0) { // Point
            float3 toLight = lightPos - worldPos;
            distance = length(toLight);
            if (lightRange > 0.0f && distance > lightRange) continue;
            L = normalize(toLight);
            attenuation = 1.0f / (distance * distance + 0.001f);
            if (lightRange > 0.0f) {
                float rangeFactor = max(0.0f, 1.0f - distance / lightRange);
                attenuation *= rangeFactor;
            }
        } else if (lightType == 1) { // Directional
            L = -lightDirection;
            distance = MAX_DISTANCE;
        } else {
            continue;
        }
        
        float NdotL = max(0.0f, dot(normal, L));
        if (NdotL <= 0.0f) continue;
        
        float shadow = 1.0f;
        if (enableShadows) {
            float3 shadowOrigin = worldPos + normal * SHADOW_BIAS;
            shadow = traceShadowRay(shadowOrigin, L, distance, triangles, numTriangles);
        }
        
        totalLight += lightColor * NdotL * attenuation * shadow;
    }
    return totalLight;
}

// ============================================================================
// Kernels
// ============================================================================

__kernel void bakeLightmap(
    __global Texel* texels,
    __global Light* lights,
    int numLights,
    __global float* triangles,
    int numTriangles,
    int enableShadows,
    __global float* outLighting
) {
    int gid = get_global_id(0);
    Texel texel = texels[gid];
    
    if (texel.valid == 0) return;
    
    float3 lighting = computeDirectLighting(
        texel.worldPos.xyz, 
        normalize(texel.normal.xyz), 
        lights, numLights, 
        triangles, numTriangles, 
        enableShadows
    );
    
    // Tonemap
    lighting = lighting / (lighting + (float3)(1.0f));
    
    outLighting[gid * 3 + 0] = lighting.x;
    outLighting[gid * 3 + 1] = lighting.y;
    outLighting[gid * 3 + 2] = lighting.z;
}

__kernel void bakeIndirect(
    __global Texel* texels,
    __global Light* lights,
    int numLights,
    __global float* triangles,
    int numTriangles,
    int enableShadows,
    int samples,
    int seedOffset,
    float intensity,
    __global float* outIndirect
) {
    int gid = get_global_id(0);
    Texel texel = texels[gid];
    
    if (texel.valid == 0) return;
    
    float3 worldPos = texel.worldPos.xyz;
    float3 normal = normalize(texel.normal.xyz);
    
    // Init RNG
    uint seed = gid + seedOffset * 719393;
    
    float3 accumulatedLight = (float3)(0.0f);
    
    for (int s = 0; s < samples; s++) {
        // Sample direction
        float3 dir = sampleHemisphere(normal, &seed);
        
        // Trace ray to find hit
        int hitTriIdx = -1;
        float hitDist = findClosestIntersection(
            worldPos + normal * SHADOW_BIAS, 
            dir, 
            triangles, numTriangles, 
            &hitTriIdx
        );
        
        if (hitTriIdx != -1) {
            // Get hit position
            float3 hitPos = worldPos + normal * SHADOW_BIAS + dir * hitDist;
            
            // Get triangle normal (Flat shading for GI approximation)
            // For better quality, we would pass vertex normals and interpolate
            // Get triangle normal
            int idx = hitTriIdx * 9;
            float3 v0 = (float3)(triangles[idx+0], triangles[idx+1], triangles[idx+2]);
            float3 v1 = (float3)(triangles[idx+3], triangles[idx+4], triangles[idx+5]);
            float3 v2 = (float3)(triangles[idx+6], triangles[idx+7], triangles[idx+8]);
            float3 faceNormal = normalize(cross(v1 - v0, v2 - v0));
            
            // Ensure normal faces the ray (Double-sided material)
            if (dot(faceNormal, dir) > 0.0f) {
                faceNormal = -faceNormal;
            }
            
            // Compute direct lighting at hit point
            float3 direct = computeDirectLighting(
                hitPos, faceNormal, 
                lights, numLights, 
                triangles, numTriangles, 
                enableShadows
            );
            
            // Clamp to reduce fireflies (e.g. max 5.0 intensity)
            accumulatedLight += min(direct, (float3)(5.0f));
        }
    }
    
    // Average
    // Factor of 2.0 comes from Monte Carlo integration over hemisphere with cosine weighted sampling
    // But typically for Lambertian it's just Average(samples). The 2.0 might be making it too bright?
    // Let's remove the * 2.0f factor for now, as standard Cosine Weighted PDF cancels out the PI and CosTheta terms, leaving just Albedo.
    // If we assume white albedo (1.0), result is just Average(Li).
    float3 indirect = accumulatedLight / (float)samples * intensity;
    
    // Write to output (additive to support multi-pass if needed, but here simple write)
    outIndirect[gid * 3 + 0] = indirect.x;
    outIndirect[gid * 3 + 1] = indirect.y;
    outIndirect[gid * 3 + 2] = indirect.z;
}
