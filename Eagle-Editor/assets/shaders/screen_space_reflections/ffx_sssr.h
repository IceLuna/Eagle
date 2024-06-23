/**********************************************************************
Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#ifndef FFX_SSSR
#define FFX_SSSR
#define FFX_SSSR_FLOAT_MAX                          3.402823466e+38

void FFX_SSSR_InitialAdvanceRay(vec3 origin, vec3 direction, vec3 inv_direction, vec2 current_mip_resolution, vec2 current_mip_resolution_inv, vec2 floor_offset, vec2 uv_offset, out vec3 position, out float current_t)
{
    vec2 current_mip_position = current_mip_resolution * origin.xy;

    // Intersect ray with the half box that is pointing away from the ray origin.
    vec2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;

    // o + d * t = p' => t = (p' - o) / d
    vec2 t = xy_plane * inv_direction.xy - origin.xy * inv_direction.xy;
    current_t = min(t.x, t.y);
    position = origin + current_t * direction;
}

bool FFX_SSSR_AdvanceRay(vec3 origin, vec3 direction, vec3 inv_direction, vec2 current_mip_position, vec2 current_mip_resolution_inv, vec2 floor_offset, vec2 uv_offset, float surface_z, inout vec3 position, inout float current_t)
{
    // Create boundary planes
    vec2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;
    vec3 boundary_planes = vec3(xy_plane, surface_z);

    // Intersect ray with the half box that is pointing away from the ray origin.
    // o + d * t = p' => t = (p' - o) / d
    vec3 t = boundary_planes * inv_direction - origin * inv_direction;

    // Prevent using z plane when shooting out of the depth buffer.
#ifdef EG_REVERSED_DEPTH // FFX_SSSR_INVERTED_DEPTH_RANGE
    t.z = direction.z < 0 ? t.z : FFX_SSSR_FLOAT_MAX;
#else
    t.z = direction.z > 0 ? t.z : FFX_SSSR_FLOAT_MAX;
#endif

    // Choose nearest intersection with a boundary.
    float t_min = min(min(t.x, t.y), t.z);

#ifdef EG_REVERSED_DEPTH // FFX_SSSR_INVERTED_DEPTH_RANGE
    // Larger z means closer to the camera.
    bool above_surface = surface_z < position.z;
#else
    // Smaller z means closer to the camera.
    bool above_surface = surface_z > position.z;
#endif

    // Decide whether we are able to advance the ray until we hit the xy boundaries or if we had to clamp it at the surface.
    // We use the asuint comparison to avoid NaN / Inf logic, also we actually care about bitwise equality here to see if t_min is the t.z we fed into the min3 above.
    bool skipped_tile = floatBitsToUint(t_min) != floatBitsToUint(t.z) && above_surface;

    // Make sure to only advance the ray if we're still above the surface.
    current_t = above_surface ? t_min : current_t;

    // Advance ray
    position = origin + current_t * direction;

    return skipped_tile;
}

vec2 FFX_SSSR_GetMipResolution(vec2 screen_dimensions, int mip_level)
{
    return screen_dimensions * pow(0.5, mip_level);
}

// Requires origin and direction of the ray to be in screen space [0, 1] x [0, 1]
vec3 FFX_SSSR_HierarchicalRaymarch(vec3 origin, vec3 direction, bool is_mirror, vec2 screen_size, int most_detailed_mip, uint min_traversal_occupancy, uint max_traversal_intersections, out bool valid_hit)
{
    const vec3 inv_direction = direction != vec3(0) ? 1.0 / direction : vec3(FFX_SSSR_FLOAT_MAX);

    // Start on mip with highest detail.
    int current_mip = most_detailed_mip;

    // Could recompute these every iteration, but it's faster to hoist them out and update them.
    vec2 current_mip_resolution = FFX_SSSR_GetMipResolution(screen_size, current_mip);
    vec2 current_mip_resolution_inv = 1 / current_mip_resolution;

    // Offset to the bounding boxes uv space to intersect the ray with the center of the next pixel.
    // This means we ever so slightly over shoot into the next region. 
    vec2 uv_offset = 0.005 * exp2(most_detailed_mip) / screen_size;
    uv_offset.x = direction.x < 0 ? -uv_offset.x : uv_offset.x;
    uv_offset.y = direction.y < 0 ? -uv_offset.y : uv_offset.y;

    // Offset applied depending on current mip resolution to move the boundary to the left/right upper/lower border depending on ray direction.
    vec2 floor_offset;
    floor_offset.x = direction.x < 0 ? 0 : 1;
    floor_offset.y = direction.y < 0 ? 0 : 1;
    
    // Initially advance ray to avoid immediate self intersections.
    float current_t;
    vec3 position;
    FFX_SSSR_InitialAdvanceRay(origin, direction, inv_direction, current_mip_resolution, current_mip_resolution_inv, floor_offset, uv_offset, position, current_t);

    bool exit_due_to_low_occupancy = false;
    int i = 0;
    while (i < max_traversal_intersections && current_mip >= most_detailed_mip && !exit_due_to_low_occupancy)
    {
        vec2 current_mip_position = current_mip_resolution * position.xy;
        float surface_z = FFX_SSSR_LoadDepth(ivec2(current_mip_position), current_mip);
        exit_due_to_low_occupancy = !is_mirror && subgroupBallotBitCount(subgroupBallot(true)) <= min_traversal_occupancy;
        bool skipped_tile = FFX_SSSR_AdvanceRay(origin, direction, inv_direction, current_mip_position, current_mip_resolution_inv, floor_offset, uv_offset, surface_z, position, current_t);
        current_mip += skipped_tile ? 1 : -1;
        current_mip_resolution *= skipped_tile ? 0.5 : 2;
        current_mip_resolution_inv *= skipped_tile ? 2 : 0.5;
        ++i;
    }

    valid_hit = (i <= max_traversal_intersections);

    return position;
}

float FFX_SSSR_ValidateHit(vec3 hit, vec2 uv, vec3 world_space_ray_direction, vec2 screen_size, float depth_buffer_thickness)
{
    // Reject hits outside the view frustum
    if (any(lessThan(hit.xy, vec2(0))) || any(greaterThan(hit.xy, vec2(1))))
    {
        return 0.f;
    }

    // Reject the hit if we didnt advance the ray significantly to avoid immediate self reflection
    vec2 manhattan_dist = abs(hit.xy - uv);
    if(all(lessThan(manhattan_dist, (2 / screen_size))))
    {
        return 0.f;
    }

    // Don't lookup radiance from the background.
    ivec2 texel_coords = ivec2(screen_size * hit.xy);
    float surface_z = FFX_SSSR_LoadDepth(texel_coords / 2, 1);
#ifdef EG_REVERSED_DEPTH // FFX_SSSR_INVERTED_DEPTH_RANGE
    if (surface_z == 0.0) {
#else
    if (surface_z == 1.0) {
#endif
        return 0.f;
    }

    // We check if we hit the surface from the back, these should be rejected.
    vec3 hit_normal = FFX_SSSR_LoadWorldSpaceNormal(texel_coords);
    if (dot(hit_normal, world_space_ray_direction) > 0)
    {
        return 0.f;
    }

    vec3 view_space_surface = FFX_SSSR_ScreenSpaceToViewSpace(vec3(hit.xy, surface_z));
    vec3 view_space_hit = FFX_SSSR_ScreenSpaceToViewSpace(hit);
    float distance = length(view_space_surface - view_space_hit);

    // Fade out hits near the screen borders
    vec2 fov = 0.05 * vec2(screen_size.y / screen_size.x, 1);
    vec2 border = smoothstep(vec2(0), fov, hit.xy) * (1 - smoothstep(1 - fov, vec2(1), hit.xy));
    float vignette = border.x * border.y;

    // We accept all hits that are within a reasonable minimum distance below the surface.
    // Add constant in linear space to avoid growing of the reflections toward the reflected objects.
    float confidence = 1 - smoothstep(0, depth_buffer_thickness, distance);
    confidence *= confidence;

    return vignette * confidence;
}

#endif //FFX_SSSR
