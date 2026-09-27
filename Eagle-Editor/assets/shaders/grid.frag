#include "defines.h"

// Infinite, anti-aliased editor grid on the y = 0 plane.
//  - Line width and anti-aliasing are computed in PIXELS using screen-space derivatives,
//    so a line is never thinner than ~1px and never aliases.
//  - Grid levels (CellSize, CellSize*10, CellSize*100, ...) are selected per pixel and per axis based on how
//    many pixels a cell covers. A level smoothly fades out before its cells get too dense to resolve,
//    so distant lines fade instead of turning into moire. This also gives infinite zoom-out.
//  - The plane is ray-traced per pixel from a fullscreen triangle, so it's infinite (limited only by the far plane).
//  - Screen-space derivatives of the hit point are computed analytically instead of with dFdx/dFdy of the
//    position. Those are one-sided differences over 2x2 quads and create a quad-aligned pattern at grazing angles.
//  - Lines are at least ~2 pixels wide (and lighter to compensate). A 1px line that moves by half a pixel
//    alternates between 1 fully covered pixel and 2 half covered ones. Since blending happens in the
//    gamma-encoded LDR output, that makes its brightness pulse by ~45% -> distant lines shimmer when moving.
//  - Depth is computed from homogeneous ray points in a form that avoids cancellation, so it keeps
//    reversed-Z precision far away (no flickering against geometry near y = 0).
//  - With TAA, depth is computed along this frame's JITTERED ray, because that's how the depth buffer it's
//    tested against was rasterized. Lines stay unjittered (the grid is drawn after TAA resolve).

layout(push_constant) uniform PushData
{
    // Columns 2 and 3 of `inverse(jittered VP) - inverse(unjittered VP)`. All zeros when TAA is off.
    // `unjitteredPoint + g_JitterDeltaZ * ndcZ + g_JitterDeltaW` is the same pixel's point on the jittered ray.
    // (Columns 0 and 1 of that difference are zero: jitter doesn't depend on NDC xy)
    layout(offset = 64) vec4 g_JitterDeltaZ;
    vec4 g_JitterDeltaW;
    float g_CellSize; // World-space size of the smallest grid cell
};

layout(location = 0) in vec4 i_NearPoint;
layout(location = 1) in vec4 i_FarPoint;

layout(location = 0) out vec4 g_Output;

// ---------------------------------- Style ----------------------------------
const vec3  s_LineColor   = vec3(0.2);
// Widths are in pixels. Keep them >= ~2: thinner lines pulse in brightness when moving by sub-pixel amounts.
// Alphas are lower than they'd be for 1px lines, so the overall look stays about as light.
const float s_MinorAlpha  = 0.22;
const float s_MajorAlpha  = 0.38;
const float s_MinorWidth  = 2.25;
const float s_MajorWidth  = 2.5;

const bool  s_DrawAxes    = true;
const vec3  s_AxisXColor  = vec3(0.85, 0.2, 0.2);  // X axis (the z = 0 line)
const vec3  s_AxisZColor  = vec3(0.2, 0.35, 0.85); // Z axis (the x = 0 line)
const float s_AxisAlpha   = 0.6;
const float s_AxisWidth   = 2.5;

// A grid level is fully faded out when its lines are this many pixels apart and fully visible at 10x that.
// Must be comfortably larger than the line widths, otherwise dense levels turn into a solid haze
const float s_MinCellPixels = 8.0;

// Fade out when the view ray is nearly parallel to the plane (sine of the angle). Hides the horizon mush.
const float s_HorizonFade = 0.03;

// Depth is written as if the plane was lifted this much towards the camera, to avoid z-fighting with floors at y = 0
const float s_DepthOffset = 0.005;
// ---------------------------------------------------------------------------

// Coverage of a line of `widthPx` pixels whose center is `distPx` pixels away
float LineCoverage(float distPx, float widthPx)
{
    return clamp(widthPx * 0.5 + 0.5 - distPx, 0.0, 1.0);
}

float LineAt(float coord, float spacing, float footprint, float widthPx)
{
    const float distWorld = abs(coord - spacing * round(coord / spacing));
    return LineCoverage(distWorld / footprint, widthPx);
}

// Lines of `coord = k * spacing` for one axis. `footprint` is how many world units one pixel covers along `coord`.
float AxisGrid(float coord, float footprint)
{
    // `lod` is the (fractional) level at which cells are exactly s_MinCellPixels apart.
    // Level L has spacing g_CellSize * 10^L. Clamped so that cells never get smaller than g_CellSize,
    // and from above so the spacing can't overflow when derivatives explode near the horizon.
    const float lod = clamp(log(s_MinCellPixels * footprint / g_CellSize) / log(10.0), -1.0, 20.0);
    const float level = floor(lod) + 1.0; // The finest level that is still resolvable

    // 1 when the finest level's cells are 10 * s_MinCellPixels apart, 0 when they're s_MinCellPixels apart.
    // Everything below is continuous across integer `lod` boundaries: when a level stops being the finest one,
    // it has already faded to 0, and the level that becomes "finest" has already transitioned from major to minor.
    const float fade = smoothstep(0.0, 1.0, 1.0 - fract(lod));

    const float spacing0 = g_CellSize * pow(10.0, level);
    const float spacing1 = spacing0 * 10.0;
    const float spacing2 = spacing1 * 10.0;

    const float line0 = LineAt(coord, spacing0, footprint, s_MinorWidth) * s_MinorAlpha * fade;
    const float line1 = LineAt(coord, spacing1, footprint, mix(s_MinorWidth, s_MajorWidth, fade)) * mix(s_MinorAlpha, s_MajorAlpha, fade);
    const float line2 = LineAt(coord, spacing2, footprint, s_MajorWidth) * s_MajorAlpha;

    // Coarser lines coincide with finer ones; use max so they don't accumulate
    return max(line0, max(line1, line2));
}

// Returns the point where the ray between two points crosses the y = 0 plane.
vec3 IntersectGrid(vec3 nearPoint, vec3 farPoint, vec3 rayDelta, out float t)
{
    const float denom = abs(rayDelta.y) > 1e-8 ? rayDelta.y : 1e-8;
    t = -nearPoint.y / denom;
    return nearPoint + clamp(t, 0.0, 1.0) * rayDelta;
}

// Exact derivative of the y = 0 hit point along one screen axis, given the derivatives of the homogeneous ray points.
// The ray points are interpolated linearly across the fullscreen triangle, so their dFdx/dFdy are exact constants.
// Differentiating the intersection analytically gives the true rate of change at this pixel's center
vec3 IntersectGridDerivative(vec4 qNear, vec4 qFar, vec4 dqNear, vec4 dqFar)
{
    const vec3 n  = qNear.xyz / qNear.w;
    const vec3 f  = qFar.xyz  / qFar.w;
    const vec3 dn = (dqNear.xyz - n * dqNear.w) / qNear.w; // Quotient rule
    const vec3 df = (dqFar.xyz  - f * dqFar.w)  / qFar.w;
    const vec3 d  = f - n;
    const vec3 dd = df - dn;
    const float dy = abs(d.y) > 1e-8 ? d.y : 1e-8;
    const float t  = -n.y / dy;
    const float dt = -(dn.y * dy - n.y * dd.y) / (dy * dy);
    return dn + dt * d + t * dd;
}

void main()
{
    const vec3 nearPoint = i_NearPoint.xyz / i_NearPoint.w;
    const vec3 farPoint  = i_FarPoint.xyz  / i_FarPoint.w;
    const vec3 rayDelta  = farPoint - nearPoint;

    float t;
    const vec3 worldPos = IntersectGrid(nearPoint, farPoint, rayDelta, t);
    const bool bHit = t > 0.0 && t <= 1.0;

    // World units per pixel, separately for x and z
    const vec4 dNearDx = dFdx(i_NearPoint), dNearDy = dFdy(i_NearPoint);
    const vec4 dFarDx  = dFdx(i_FarPoint),  dFarDy  = dFdy(i_FarPoint);
    const vec3 dPosDx = IntersectGridDerivative(i_NearPoint, i_FarPoint, dNearDx, dFarDx);
    const vec3 dPosDy = IntersectGridDerivative(i_NearPoint, i_FarPoint, dNearDy, dFarDy);
    const vec2 footprint = max(vec2(
        length(vec2(dPosDx.x, dPosDy.x)),
        length(vec2(dPosDx.z, dPosDy.z))), vec2(1e-8));

    // Depth, where the ray hits the plane lifted by s_DepthOffset (towards the camera, to avoid z-fighting with floors).
    // Uses the jittered ray: with TAA, the depth buffer was rendered jittered by up to half a pixel, and at grazing
    // angles half a pixel changes a floor's depth by more than the lift -> the grid would randomly fail the test.
    // Computed on the homogeneous ray points: for Q(s) = mix(qNear, qFar, s), VP * Q(s) = (ndc.xy, mix(near, far, s), 1),
    // so NDC depth is linear in `s`: depth = near * (1 - s) + far * s.
    // `1 - s` is NOT computed as a subtraction: far away `s` is close to 1 and `1 - s` would lose most of the
    // precision reversed-Z gives. Instead `s` and `1 - s` are both formed as numerator / denominator directly.
    {
        const vec4 qNear = i_NearPoint + g_JitterDeltaZ * EG_DEPTH_NEAR + g_JitterDeltaW;
        const vec4 qFar  = i_FarPoint  + g_JitterDeltaZ * EG_DEPTH_FAR  + g_JitterDeltaW;
        const float h = nearPoint.y > 0.0 ? s_DepthOffset : -s_DepthOffset;
        const float sNum         = h * qNear.w - qNear.y; // s     = sNum / den
        const float oneMinusSNum = qFar.y - h * qFar.w;   // 1 - s = oneMinusSNum / den
        const float den = sNum + oneMinusSNum;
        const float safeDen = abs(den) > 1e-30 ? den : 1e-30;
        gl_FragDepth = clamp((EG_DEPTH_NEAR * oneMinusSNum + EG_DEPTH_FAR * sNum) / safeDen, 0.0, 1.0);
    }

    // Grid
    const float gridAlpha = max(AxisGrid(worldPos.x, footprint.x), AxisGrid(worldPos.z, footprint.y));
    vec4 color = vec4(s_LineColor, gridAlpha);

    if (s_DrawAxes)
    {
        const float axisZ = LineCoverage(abs(worldPos.x) / footprint.x, s_AxisWidth) * s_AxisAlpha; // x = 0
        const float axisX = LineCoverage(abs(worldPos.z) / footprint.y, s_AxisWidth) * s_AxisAlpha; // z = 0
        color.rgb = mix(color.rgb, s_AxisZColor, axisZ);
        color.rgb = mix(color.rgb, s_AxisXColor, axisX);
        color.a = max(color.a, max(axisX, axisZ));
    }

    // Fade out near the horizon and before the far plane (so there's no hard cut at the far clip)
    const float rayDirY = abs(rayDelta.y) / max(length(rayDelta), 1e-8);
    color.a *= smoothstep(0.0, s_HorizonFade, rayDirY);
    color.a *= 1.0 - smoothstep(0.8, 1.0, t);

    if (!bHit || color.a <= 0.0)
        discard;

    g_Output = color;
}
