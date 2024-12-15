#ifndef EG_UTILS
#define EG_UTILS

#extension GL_KHR_shader_subgroup_ballot : enable

// For each component of v, returns -1 if the component is < 0, else 1
vec2 sign_not_zero(vec2 v)
{
    return vec2(
        v.x >= 0.f ? 1.0f : -1.0f,
        v.y >= 0.f ? 1.0f : -1.0f
    );
}

/*
    Normal packing as described in:
    A Survey of Efficient Representations for Independent Unit Vectors
    Source: http://jcgt.org/published/0003/02/01/paper.pdf
    4 functions
*/

// 1) Packs a 3-component normal to 2 channels using octahedron normals
vec2 pack_normal_octahedron(vec3 v)
{
    v.xy /= (abs(v.x) + abs(v.y) + abs(v.z));

    if (v.z <= 0)
        v.xy = (1.0 - abs(v.yx)) * sign_not_zero(v.xy);
    
    return v.xy;
}

// 2) Unpacking from octahedron normals, input is the output from pack_normal_octahedron
vec3 unpack_normal_octahedron(vec2 packed_nrm)
{
    vec3 v = vec3(packed_nrm.xy, 1.0f - abs(packed_nrm.x) - abs(packed_nrm.y));
    if (v.z < 0)
        v.xy = (1.0f - abs(v.yx)) * sign_not_zero(v.xy);

    return normalize(v);
}

/* 3) The caller should store the return value into a GL_RGB8 texture
or attribute without modification. */
vec3 snorm12x2_to_unorm8x3(vec2 f)
{
    vec2 u = vec2(round(clamp(f, -1.0, 1.0) * 2047 + 2047));
    float t = floor(u.y / 256.0);
    // If storing to GL_RGB8UI, omit the final division
    return floor(vec3(u.x / 16.0,
        fract(u.x / 16.0) * 256.0 + t,
        u.y - t * 256.0)) / 255.0;
}

// 4th function
vec2 unorm8x3_to_snorm12x2(vec3 u)
{
    u *= 255.0;
    u.y *= (1.0 / 16.0);
    vec2 s = vec2(u.x * 16.0 + floor(u.y),
        fract(u.y) * (16.0 * 256.0) + u.z);
    return clamp(s * (1.0 / 2047.0) - 1.0, vec2(-1.0), vec2(1.0));
}

vec2 EncodeNormal(vec3 normal)
{
    return pack_normal_octahedron(normal);
}

vec3 DecodeNormal(vec2 normal)
{
    return unpack_normal_octahedron(normal);
}

vec3 WorldPosFromDepth(mat4 VPInv, vec2 uv, float depth)
{
    const vec4 clipSpacePos = vec4(uv * 2.f - 1.f, depth, 1.0);
    vec4 worldSpacePos = VPInv * clipSpacePos;
    worldSpacePos /= worldSpacePos.w;

    return worldSpacePos.xyz;
}

vec3 ViewPosFromDepth(mat4 projInv, vec2 uv, float depth)
{
    const vec4 clipSpacePos = vec4(uv * 2.f - 1.f, depth, 1.0);
    vec4 viewSpacePos = projInv * clipSpacePos;
    viewSpacePos /= viewSpacePos.w;

    return viewSpacePos.xyz;
}

float VectorToDepth(vec3 val, float n, float f)
{
    const vec3 absVec = abs(val);
    const float localZcomp = max(absVec.x, max(absVec.y, absVec.z));

    const float normZComp = (f + n) / (f - n) - (2.f * f * n) / (f - n) / localZcomp;
    return normZComp * 0.5f + 0.5f;
}

float EG_min3(float a, float b, float c)
{
    return min(a, min(b, c));
}

vec2 EG_min3(vec2 a, vec2 b, vec2 c)
{
    return min(a, min(b, c));
}

vec3 EG_min3(vec3 a, vec3 b, vec3 c)
{
    return min(a, min(b, c));
}

vec4 EG_min3(vec4 a, vec4 b, vec4 c)
{
    return min(a, min(b, c));
}

float EG_max3(float a, float b, float c)
{
    return max(a, max(b, c));
}

vec2 EG_max3(vec2 a, vec2 b, vec2 c)
{
    return max(a, max(b, c));
}

vec3 EG_max3(vec3 a, vec3 b, vec3 c)
{
    return max(a, max(b, c));
}

vec4 EG_max3(vec4 a, vec4 b, vec4 c)
{
    return max(a, max(b, c));
}

float EG_med3(float a, float b, float c)
{
    return a + b + c - EG_min3(a, b, c) - EG_max3(a, b, c);
}

vec2 EG_med3(vec2 a, vec2 b, vec2 c)
{
    return a + b + c - EG_min3(a, b, c) - EG_max3(a, b, c);
}

vec3 EG_med3(vec3 a, vec3 b, vec3 c)
{
    return a + b + c - EG_min3(a, b, c) - EG_max3(a, b, c);
}

vec4 EG_med3(vec4 a, vec4 b, vec4 c)
{
    return a + b + c - EG_min3(a, b, c) - EG_max3(a, b, c);
}

void sort3(inout float p1, inout float p2, inout float p3)
{
    float minValue = EG_min3(p1, p2, p3);
    float medValue = EG_med3(p1, p2, p3);
    float maxValue = EG_max3(p1, p2, p3);

    p1 = minValue;
    p2 = medValue;
    p3 = maxValue;
}

void sort3(inout vec3 p1, inout vec3 p2, inout vec3 p3)
{
    vec3 minValue = EG_min3(p1, p2, p3);
    vec3 medValue = EG_med3(p1, p2, p3);
    vec3 maxValue = EG_max3(p1, p2, p3);

    p1 = minValue;
    p2 = medValue;
    p3 = maxValue;
}

float ToLinear(float d, float far, float near)
{
    return near * far / (far + d * (near - far));
}

uvec2 Unflatten2D(uint idx, uvec2 dim)
{
    return uvec2(idx % dim.x, idx / dim.x);
}

struct CullingFrustum
{
    float NearRight;
    float NearTop;
    float NearPlane;
    float FarPlane;
};

struct OBB
{
    // Orthonormal basis
    vec3 Axes[3];
    vec3 Center;
    vec3 Extents;
};

// Source: https://bruop.github.io/improved_frustum_culling/
// vsTransform = g_View * g_World;
// AABB is in local space
bool FrustumAABBIntersection(CullingFrustum frustum, mat4 vsTransform, vec3 aabbMin, vec3 aabbMax)
{
    // Near, far
    float zNear = frustum.NearPlane;
    float zFar = frustum.FarPlane;
    // half width, half height
    float xNear = frustum.NearRight;
    float yNear = frustum.NearTop;

    // So first thing we need to do is obtain the normal directions of our OBB by transforming 4 of our AABB vertices
    vec3 corners[] = {
        {aabbMin.x, aabbMin.y, aabbMin.z},
        {aabbMax.x, aabbMin.y, aabbMin.z},
        {aabbMin.x, aabbMax.y, aabbMin.z},
        {aabbMin.x, aabbMin.y, aabbMax.z}
    };

    // Transform corners
    // This only translates to our OBB if our transform is affine
    for (uint i = 0; i < 4; i++)
    {
        corners[i] = (vsTransform * vec4(corners[i], 1.f)).xyz;
    }

    OBB obb;
    obb.Axes[0] = corners[1] - corners[0];
    obb.Axes[1] = corners[2] - corners[0];
    obb.Axes[2] = corners[3] - corners[0];
    obb.Center = corners[0] + 0.5f * (obb.Axes[0] + obb.Axes[1] + obb.Axes[2]);
    obb.Extents = vec3(length(obb.Axes[0]), length(obb.Axes[1]), length(obb.Axes[2]));
    obb.Axes[0] = obb.Axes[0] / obb.Extents.x;
    obb.Axes[1] = obb.Axes[1] / obb.Extents.y;
    obb.Axes[2] = obb.Axes[2] / obb.Extents.z;
    obb.Extents *= 0.5f;

    {
        vec3 M = vec3(0, 0, 1);
        float MoX = 0.0f;
        float MoY = 0.0f;
        float MoZ = 1.0f;

        // Projected center of our OBB
        float MoC = obb.Center.z;
        // Projected size of OBB
        float radius = 0.0f;
        for (uint i = 0; i < 3; i++)
        {
            radius += abs(obb.Axes[i].z) * obb.Extents[i];
        }
        float obbMin = MoC - radius;
        float obbMax = MoC + radius;

        float tau0 = zFar; // Since z is negative, far is smaller than near
        float tau1 = zNear;

        if (obbMin > tau1 || obbMax < tau0) {
            return false;
        }
    }

    {
        const vec3 M[] = {
            vec3(zNear, 0.0f, xNear), // Left Plane
            vec3(-zNear, 0.0f, xNear), // Right plane
            vec3(0.0, -zNear, yNear), // Top plane
            vec3(0.0, zNear, yNear) // Bottom plane
        };

        for (uint m = 0; m < 4; m++)
        {
            float MoX = abs(M[m].x);
            float MoY = abs(M[m].y);
            float MoZ = M[m].z;
            float MoC = dot(M[m], obb.Center);

            float obbRadius = 0.0f;
            for (uint i = 0; i < 3; i++)
                obbRadius += abs(dot(M[m], obb.Axes[i])) * obb.Extents[i];

            float obbMin = MoC - obbRadius;
            float obbMax = MoC + obbRadius;

            float p = xNear * MoX + yNear * MoY;

            float tau0 = zNear * MoZ - p;
            float tau1 = zNear * MoZ + p;

            if (tau0 < 0.0f)
                tau0 *= zFar / zNear;
            if (tau1 > 0.0f)
                tau1 *= zFar / zNear;

            if (obbMin > tau1 || obbMax < tau0)
                return false;
        }
    }

    // OBB Axes
    {
        for (uint m = 0; m < 3; m++)
        {
            vec3 M = obb.Axes[m];
            float MoX = abs(M.x);
            float MoY = abs(M.y);
            float MoZ = M.z;
            float MoC = dot(M, obb.Center);

            float obbRadius = obb.Extents[m];

            float obbMin = MoC - obbRadius;
            float obbMax = MoC + obbRadius;

            // Frustum projection
            float p = xNear * MoX + yNear * MoY;
            float tau0 = zNear * MoZ - p;
            float tau1 = zNear * MoZ + p;
            if (tau0 < 0.0f)
                tau0 *= zFar / zNear;
            if (tau1 > 0.0f)
                tau1 *= zFar / zNear;

            if (obbMin > tau1 || obbMax < tau0)
                return false;
        }
    }

    // Now let's perform each of the cross products between the edges
    // First R x A_i
    {
        for (uint m = 0; m < 3; m++)
        {
            const vec3 M = vec3(0.0f, -obb.Axes[m].z, obb.Axes[m].y);
            float MoX = 0.0f;
            float MoY = abs(M.y);
            float MoZ = M.z;
            float MoC = M.y * obb.Center.y + M.z * obb.Center.z;

            float obbRadius = 0.0f;
            for (uint i = 0; i < 3; i++)
                obbRadius += abs(dot(M, obb.Axes[i])) * obb.Extents[i];

            float obbMin = MoC - obbRadius;
            float obbMax = MoC + obbRadius;

            // Frustum projection
            float p = xNear * MoX + yNear * MoY;
            float tau0 = zNear * MoZ - p;
            float tau1 = zNear * MoZ + p;
            if (tau0 < 0.0f)
                tau0 *= zFar / zNear;
            if (tau1 > 0.0f)
                tau1 *= zFar / zNear;

            if (obbMin > tau1 || obbMax < tau0)
                return false;
        }
    }

    // U x A_i
    {
        for (uint m = 0; m < 3; m++)
        {
            const vec3 M = { obb.Axes[m].z, 0.0f, -obb.Axes[m].x };
            float MoX = abs(M.x);
            float MoY = 0.0f;
            float MoZ = M.z;
            float MoC = M.x * obb.Center.x + M.z * obb.Center.z;

            float obbRadius = 0.0f;
            for (uint i = 0; i < 3; i++)
                obbRadius += abs(dot(M, obb.Axes[i])) * obb.Extents[i];

            float obbMin = MoC - obbRadius;
            float obbMax = MoC + obbRadius;

            // Frustum projection
            float p = xNear * MoX + yNear * MoY;
            float tau0 = zNear * MoZ - p;
            float tau1 = zNear * MoZ + p;
            if (tau0 < 0.0f)
                tau0 *= zFar / zNear;
            if (tau1 > 0.0f)
                tau1 *= zFar / zNear;

            if (obbMin > tau1 || obbMax < tau0)
                return false;
        }
    }

    // Frustum Edges X Ai
    {
        for (uint obbEdgeIdx = 0; obbEdgeIdx < 3; obbEdgeIdx++)
        {
            const vec3 M[] = {
                cross(vec3(-xNear, 0.0f, zNear), obb.Axes[obbEdgeIdx]), // Left Plane
                cross(vec3(xNear, 0.0f, zNear), obb.Axes[obbEdgeIdx]), // Right plane
                cross(vec3(0.0f, yNear, zNear), obb.Axes[obbEdgeIdx]), // Top plane
                cross(vec3(0.0, -yNear, zNear), obb.Axes[obbEdgeIdx]) // Bottom plane
            };

            for (uint m = 0; m < 4; m++)
            {
                float MoX = abs(M[m].x);
                float MoY = abs(M[m].y);
                float MoZ = M[m].z;

                const float epsilon = 1e-4;
                if (MoX < epsilon && MoY < epsilon && abs(MoZ) < epsilon)
                    continue;

                float MoC = dot(M[m], obb.Center);

                float obbRadius = 0.0f;
                for (uint i = 0; i < 3; i++)
                    obbRadius += abs(dot(M[m], obb.Axes[i])) * obb.Extents[i];

                float obbMin = MoC - obbRadius;
                float obbMax = MoC + obbRadius;

                // Frustum projection
                float p = xNear * MoX + yNear * MoY;
                float tau0 = zNear * MoZ - p;
                float tau1 = zNear * MoZ + p;
                if (tau0 < 0.0f)
                    tau0 *= zFar / zNear;
                if (tau1 > 0.0f)
                    tau1 *= zFar / zNear;

                if (obbMin > tau1 || obbMax < tau0)
                    return false;
            }
        }
    }

    // No intersections detected
    return true;
}

vec3 BarycentricInterp(vec3 v0, vec3 v1, vec3 v2, vec2 buv)
{
    return v0 * (1.f - buv.x - buv.y) + v1 * buv.x + v2 * buv.y;
}

#define EG_SUBGROUP_ATOMIC_INCREMENT(data, bActive, outputIndex) \
{ \
    const uvec4 activeLanes = subgroupBallot(bActive); \
    const uint activeLanesCount = subgroupBallotBitCount(activeLanes); \
    \
    uint waveStartIndex; \
    if (subgroupElect()) \
    { \
        waveStartIndex = atomicAdd(data, activeLanesCount); \
    } \
    waveStartIndex = subgroupBroadcastFirst(waveStartIndex); \
    \
    uint localIndex = subgroupBallotExclusiveBitCount(activeLanes); \
    \
    outputIndex = waveStartIndex + localIndex; \
}

#define EG_SUBGROUP_ATOMIC_DECREMENT(data, bActive, outputIndex, origValue) \
{ \
    const uvec4 activeLanes = subgroupBallot(bActive); \
    const uint activeLanesCount = subgroupBallotBitCount(activeLanes); \
    \
    uint waveStartIndex; \
    if (subgroupElect()) \
    { \
        waveStartIndex = atomicAdd(data, (~activeLanesCount) + 1u); \
    } \
    waveStartIndex = subgroupBroadcastFirst(waveStartIndex); \
    origValue = waveStartIndex; \
    \
    uint localIndex = subgroupBallotExclusiveBitCount(activeLanes); \
    \
    outputIndex = waveStartIndex - localIndex; \
}

#endif
