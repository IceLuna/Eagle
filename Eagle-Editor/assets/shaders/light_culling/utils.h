// Sources:
// https://www.3dgep.com/forward-plus/#forward
// https://wickedengine.net/ Blogs & source code
// https://simoncoenen.com/blog/programming/graphics/SpotlightCulling

#ifndef EG_LIGHT_CULLING_UTILS
#define EG_LIGHT_CULLING_UTILS

struct Sphere
{
    vec3  c; // Center point.
    float r; // Radius.
};

struct Cone
{
    vec3  T;   // Cone tip.
    float h;   // Height of the cone.
    vec3  d;   // Direction of the cone.
    float r;   // bottom radius of the cone.
};

struct Plane
{
    vec3  N; // Plane normal.
    float d; // Distance to origin.
};

struct AABB
{
    vec3 c;
    vec3 e;
};

// Four planes of a view frustum (in view space).
// The planes are:
//  * Left,
//  * Right,
//  * Top,
//  * Bottom.
// The back and/or front planes can be computed from depth values in the 
// light culling compute shader.
struct Frustum
{
    Plane planes[4]; // left, right, top, bottom frustum planes.
};

#ifndef __cplusplus
#include "common_structures.h"
#endif

// Convert clip space coordinates to view space
vec4 ClipToView(mat4 invProj, vec4 clip)
{
    // View space position.
    vec4 view = invProj * clip;
    // Perspective projection.
    view = view / view.w;

    return view;
}

// Convert screen space coordinates to view space.
vec4 ScreenToView(mat4 invProj, vec4 screen, vec2 rcpScreenDims)
{
    // Convert to normalized texture coordinates
    vec2 texCoord = vec2(screen) * rcpScreenDims;

    // Convert to clip space
    vec4 clip = vec4(vec2(texCoord.x, texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);

    return ClipToView(invProj, clip);
}

// Compute a plane from 3 noncollinear points that form a triangle.
// This equation assumes a right-handed (counter-clockwise winding order) 
// coordinate system to determine the direction of the plane normal.
Plane ComputePlane(vec3 p0, vec3 p1, vec3 p2)
{
    Plane plane;

    vec3 v0 = p1 - p0;
    vec3 v2 = p2 - p0;

    const vec3 n = normalize(cross(v0, v2));

    // Compute the distance to the origin using p0.
    plane.d = dot(n, p0);
    plane.N = -n;

    return plane;
}

AABB AABBfromMinMax(vec3 minAABB, vec3 maxAABB)
{
    AABB aabb;
    aabb.c = (minAABB + maxAABB) * 0.5f;
    aabb.e = abs(maxAABB - aabb.c);
    return aabb;
}

bool SphereIntersectsAABB(Sphere sphere, AABB aabb)
{
    vec3 vDelta = max(vec3(0), abs(aabb.c - sphere.c) - aabb.e);
    float fDistSq = dot(vDelta, vDelta);
    return fDistSq <= sphere.r * sphere.r;
}

// Check to see if a point is fully behind (inside the negative halfspace of) a plane.
bool PointInsidePlane(vec3 p, Plane plane)
{
    return dot(plane.N, p) - plane.d < 0;
}

// Check to see if a sphere is fully behind (inside the negative halfspace of) a plane.
// Source: Real-time collision detection, Christer Ericson (2005)
bool SphereInsidePlane(Sphere sphere, Plane plane)
{
    return dot(plane.N, sphere.c) - plane.d < -sphere.r;
}

// Check to see if a cone if fully behind (inside the negative halfspace of) a plane.
// Source: Real-time collision detection, Christer Ericson (2005)
bool ConeInsidePlane(Cone cone, Plane plane)
{
    // Base center
    const vec3 C = cone.T + cone.d * cone.h;

    // Project plane normal onto cone base plane
    vec3 m = plane.N - cone.d * dot(plane.N, cone.d);
    const float len2 = dot(m, m);

    vec3 Q;
    if (len2 > 1e-6f)
    {
        m = m / sqrt(len2);  // normalize
        Q = C + m * cone.r;  // extreme point on base circle
    }
    else
    {
        // Plane normal is parallel to cone axis
        Q = C;
    }

    // Check both tip and extreme base point
    return PointInsidePlane(cone.T, plane) &&
        PointInsidePlane(Q, plane);
}

// Check to see of a light is partially contained within the frustum.
bool SphereInsideFrustum(Sphere sphere, Frustum frustum, float zNear, float zFar)
{
    bool bResult = true;
    bResult = (zNear < (sphere.c.z - sphere.r) || (sphere.c.z + sphere.r) < zFar) ? false : bResult;
    bResult = ((SphereInsidePlane(sphere, frustum.planes[0])) ? false : bResult);
    bResult = ((SphereInsidePlane(sphere, frustum.planes[1])) ? false : bResult);
    bResult = ((SphereInsidePlane(sphere, frustum.planes[2])) ? false : bResult);
    bResult = ((SphereInsidePlane(sphere, frustum.planes[3])) ? false : bResult);

    return bResult;
}

bool ConeInsideFrustum(Cone cone, Frustum frustum, float zNear, float zFar)
{
    Plane nearPlane = { vec3(0, 0, -1), zNear };
    Plane farPlane = { vec3(0, 0, 1), zFar };

    // First check the near and far clipping planes.
    if (ConeInsidePlane(cone, nearPlane) || ConeInsidePlane(cone, farPlane))
    {
        return false;
    }

    // Then check frustum planes
    for (int i = 0; i < 4; i++)
    {
        if (ConeInsidePlane(cone, frustum.planes[i]))
        {
            return false;
        }
    }

    return true;
}

#ifndef __cplusplus
void CalculateFrustumAndAABB(mat4 invProj, vec2 rcpScreenSize, vec2 minSize, vec2 maxSize, float minDepth, float maxDepth, out Frustum frustum, out AABB aabb)
#else
void CalculateFrustumAndAABB(mat4 invProj, vec2 rcpScreenSize, vec2 minSize, vec2 maxSize, float minDepth, float maxDepth, Frustum& frustum, AABB& aabb)
#endif
{
    vec3 viewSpace[8];
    // Top left point, near
    viewSpace[0] = vec3(ScreenToView(invProj, vec4(minSize, minDepth, 1.0f), rcpScreenSize));
    // Top right point, near
    viewSpace[1] = vec3(ScreenToView(invProj, vec4(maxSize.x, minSize.y, minDepth, 1.0f), rcpScreenSize));
    // Bottom left point, near
    viewSpace[2] = vec3(ScreenToView(invProj, vec4(minSize.x, maxSize.y, minDepth, 1.0f), rcpScreenSize));
    // Bottom right point, near
    viewSpace[3] = vec3(ScreenToView(invProj, vec4(maxSize.x, maxSize.y, minDepth, 1.0f), rcpScreenSize));

    // Top left point, far
    viewSpace[4] = vec3(ScreenToView(invProj, vec4(minSize, maxDepth, 1.0f), rcpScreenSize));
    // Top right point, far
    viewSpace[5] = vec3(ScreenToView(invProj, vec4(maxSize.x, minSize.y, maxDepth, 1.0f), rcpScreenSize));
    // Bottom left point, far
    viewSpace[6] = vec3(ScreenToView(invProj, vec4(minSize.x, maxSize.y, maxDepth, 1.0f), rcpScreenSize));
    // Bottom right point, far
    viewSpace[7] = vec3(ScreenToView(invProj, vec4(maxSize.x, maxSize.y, maxDepth, 1.0f), rcpScreenSize));

    // Left plane
    frustum.planes[0] = ComputePlane(viewSpace[2], viewSpace[0], viewSpace[4]);
    // Right plane
    frustum.planes[1] = ComputePlane(viewSpace[1], viewSpace[3], viewSpace[5]);
    // Top plane
    frustum.planes[2] = ComputePlane(viewSpace[0], viewSpace[1], viewSpace[4]);
    // Bottom plane
    frustum.planes[3] = ComputePlane(viewSpace[3], viewSpace[2], viewSpace[6]);

    // I construct an AABB around the minmax depth bounds to perform tighter culling:
    // The frustum is asymmetric so we must consider all corners!
    vec3 minAABB = vec3(10000000);
    vec3 maxAABB = vec3(-10000000);
    for (int i = 0; i < 8; ++i)
    {
        minAABB = min(minAABB, viewSpace[i]);
        maxAABB = max(maxAABB, viewSpace[i]);
    }

    aabb = AABBfromMinMax(minAABB, maxAABB);
}

#ifndef __cplusplus
Sphere SphereFromPointLight(PointLight light, mat4 view)
#else
Sphere SphereFromPointLight(const PointLight& light, mat4 view)
#endif
{
    const vec3 posVS = vec3(view * vec4(light.Position, 1));
    const float radius = sqrt(abs(light.Radius2)); // `abs` because sign bit is used as a flag for `bCastsShadows`

    Sphere sphere;
    sphere.c = posVS;
    sphere.r = radius;

    return sphere;
}

#ifndef __cplusplus
Cone ConeFromSpotLight(SpotLight light, mat4 view)
#else
Cone ConeFromSpotLight(const SpotLight& light, mat4 view)
#endif
{
    const float distance = light.Distance;
    const float radius = distance * tan(light.OuterCutOffRadians);

    Cone cone;
    cone.T = vec3(view * vec4(light.Position, 1));
    cone.d = mat3(view) * light.Direction;
    cone.h = distance;
    cone.r = radius;

    return cone;
}

#ifndef __cplusplus
Sphere SphereFromSpotLight(SpotLight light, mat4 view)
#else
Sphere SphereFromSpotLight(const SpotLight& light, mat4 view)
#endif
{
    const float distance = light.Distance;
    const float angleCos = cos(light.OuterCutOffRadians);

    const vec3 posVS = vec3(view * vec4(light.Position, 1));
    const vec3 dirVS = normalize(mat3(view) * light.Direction);
    const float r = distance * 0.5f / (angleCos * angleCos);

    Sphere sphere;
    sphere.c = posVS + dirVS * r;
    sphere.r = r;

    return sphere;
}

// Constructs the depth mask for the sphere that represents the area (depth range) it affects
uint GetSphereDepthMask(Sphere sphere, float minDepthVS, float rcpDepthRange)
{
    const float depthMin = sphere.c.z + sphere.r;
    const float depthMax = sphere.c.z - sphere.r;
#ifndef __cplusplus
    const uint depthCellIndexStart = clamp(uint(floor((depthMin - minDepthVS) * rcpDepthRange)), 0u, 31u);
    const uint depthCellIndexEnd = clamp(uint(floor((depthMax - minDepthVS) * rcpDepthRange)), 0u, 31u);
#else
    const uint depthCellIndexStart = glm::clamp(uint(floor((depthMin - minDepthVS) * rcpDepthRange)), 0u, 31u);
    const uint depthCellIndexEnd = glm::clamp(uint(floor((depthMax - minDepthVS) * rcpDepthRange)), 0u, 31u);
#endif

    //// Unoptimized mask construction with loop:
    //// Construct mask from START to END:
    ////          END         START
    ////	0000000000111111111110000000000
    //uint lightDepthMask = 0;
    //for (uint c = depthCellIndexStart; c <= depthCellIndexEnd; ++c)
    //{
    //	lightDepthMask |= 1u << c;
    //}

    // Optimized mask construction, without loop:
    //	- First, fill full mask:
    //	1111111111111111111111111111111
    uint lightDepthMask = 0xFFFFFFFF;
    //	- Then Shift right with spare amount to keep mask only:
    //	0000000000000000000011111111111
    lightDepthMask >>= 31u - (depthCellIndexEnd - depthCellIndexStart);
    //	- Last, shift left with START amount to correct mask position:
    //	0000000000111111111110000000000
    lightDepthMask <<= depthCellIndexStart;

    return lightDepthMask;
}

bool SpotIntersectsAABB(SpotLight light, AABB aabb, mat4 view)
{
    const vec3 pos = vec3(view * vec4(light.Position, 1));
    const vec3 dir = mat3(view) * light.Direction;

    float sphereRadius = length(aabb.e);
    vec3 v = aabb.c - pos;
    float lenSq = dot(v, v);
    float v1Len = dot(v, dir);
    float distanceClosestPoint = cos(light.OuterCutOffRadians) * sqrt(lenSq - v1Len * v1Len) - v1Len * sin(light.OuterCutOffRadians);
    bool angleCull = distanceClosestPoint > sphereRadius;
    bool frontCull = v1Len > sphereRadius + light.Distance;
    bool backCull = v1Len < -sphereRadius;
    return !(angleCull || frontCull || backCull);
}

#endif // EG_LIGHT_CULLING_UTILS
