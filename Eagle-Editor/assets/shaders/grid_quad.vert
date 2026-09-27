#include "defines.h"

layout(push_constant) uniform PushData
{
    mat4 g_InvViewProj;
};

layout(location = 0) out vec4 o_NearPoint; // Homogeneous world-space point on the near plane
layout(location = 1) out vec4 o_FarPoint;  // Homogeneous world-space point on the far plane

void main()
{
    const vec2 ndc = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.5, 1.0); // Z doesn't matter, the fragment shader writes the real depth

    // The points are NOT divided by w here. Since every vertex has w == 1, varyings are interpolated linearly
    // in screen space, which exactly matches `g_InvViewProj * vec4(pixelNDC, z, 1)` (it's affine in NDC).
    // The division happens per-pixel in the fragment shader.
    o_NearPoint = g_InvViewProj * vec4(ndc, EG_DEPTH_NEAR, 1.0);
    o_FarPoint  = g_InvViewProj * vec4(ndc, EG_DEPTH_FAR, 1.0);
}
