#version 450

// Instance attributes
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inSize; 
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec4 inUV; // u0, v0, u1, v1

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} pc;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

void main() {
    outColor = inColor;
    
    // Generate quad vertices from gl_VertexIndex (0..5)
    // CW Winding for Backface Culling
    // 0: TL, 1: TR, 2: BL
    // 3: BL, 4: TR, 5: BR
    
    vec2 pos;
    vec2 uv;
    
    // UV Mapping
    if (gl_VertexIndex == 0) { uv = vec2(inUV.x, inUV.y); } // TL
    else if (gl_VertexIndex == 1) { uv = vec2(inUV.z, inUV.y); } // TR
    else if (gl_VertexIndex == 2) { uv = vec2(inUV.x, inUV.w); } // BL
    else if (gl_VertexIndex == 3) { uv = vec2(inUV.x, inUV.w); } // BL
    else if (gl_VertexIndex == 4) { uv = vec2(inUV.z, inUV.y); } // TR
    else { uv = vec2(inUV.z, inUV.w); } // BR
    
    outUV = uv;
    
    // Position Calculation
    // Map 0..1 quad to world pos/size
    vec2 localPos;
    if (gl_VertexIndex == 0) { localPos = vec2(0, 0); }
    else if (gl_VertexIndex == 1) { localPos = vec2(1, 0); }
    else if (gl_VertexIndex == 2) { localPos = vec2(0, 1); }
    else if (gl_VertexIndex == 3) { localPos = vec2(0, 1); }
    else if (gl_VertexIndex == 4) { localPos = vec2(1, 0); }
    else { localPos = vec2(1, 1); }
    
    vec2 worldPos = inPos + localPos * inSize;
    
    // Convert to NDC (-1..1)
    // 0,0 is top-left in screen coordinates
    // Vulkan NDC: -1,-1 is top-left? No. -1,-1 is top-left in OGL?
    // Vulkan: (-1, -1) is Top-Left if we map it directly?
    // Screen coords: 0..W, 0..H.
    // Normalized: 0..1, 0..1.
    // NDC x: (x / W) * 2 - 1
    // NDC y: (y / H) * 2 - 1. 
    // BUT Vulkan Y is down? 
    // If (0,0) is top-left pixel.
    // NDC (-1, -1) should be top-left.
    
    float x = (worldPos.x / pc.screenSize.x) * 2.0 - 1.0;
    float y = (worldPos.y / pc.screenSize.y) * 2.0 - 1.0;
    
    gl_Position = vec4(x, y, 0.0, 1.0);
}
