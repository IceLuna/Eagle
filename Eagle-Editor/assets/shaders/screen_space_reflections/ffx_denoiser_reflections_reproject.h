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

#ifndef FFX_DNSR_REFLECTIONS_REPROJECT
#define FFX_DNSR_REFLECTIONS_REPROJECT

#define FFX_DNSR_REFLECTIONS_ESTIMATES_LOCAL_NEIGHBORHOOD
#include "screen_space_reflections/ffx_denoiser_reflections_common.h"

shared uint g_ffx_dnsr_shared_0[16][16];
shared uint g_ffx_dnsr_shared_1[16][16];

struct FFX_DNSR_Reflections_NeighborhoodSample
{
    vec3 radiance;
};

FFX_DNSR_Reflections_NeighborhoodSample FFX_DNSR_Reflections_LoadFromGroupSharedMemory(ivec2 idx)
{
    uvec2       packed_radiance      = uvec2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    vec4 unpacked_radiance        = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);

    FFX_DNSR_Reflections_NeighborhoodSample smpl;
    smpl.radiance = unpacked_radiance.xyz;
    return smpl;
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(ivec2 group_thread_id, vec3 radiance)
{
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(ivec2 group_thread_id, vec4 radiance_variance) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance_variance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance_variance.zw);
}

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size)
{
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    ivec2 offset[4] = { ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    vec3 radiance[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (int i = 0; i < 4; ++i)
    {
        radiance[i] = FFX_DNSR_Reflections_LoadRadiance(dispatch_thread_id + offset[i]);
    }

    // Then move all registers to groupshared memory
    for (int j = 0; j < 4; ++j)
    {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j]); // X
    }
}

vec4 FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(ivec2 idx)
{
    uvec2 packed_radiance = uvec2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    return FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);
}

float FFX_DNSR_Reflections_GetLuminanceWeight(vec3 val)
{
    float luma   = FFX_DNSR_Reflections_Luminance(val.xyz);
    float weight = max(exp(-luma * FFX_DNSR_REFLECTIONS_AVG_RADIANCE_LUMINANCE_WEIGHT), 1.0e-2);
    return weight;
}

vec2 FFX_DNSR_Reflections_GetSurfaceReprojection(ivec2 dispatch_thread_id, vec2 uv, vec2 motion_vector)
{
    // Reflector position reprojection
    vec2 history_uv = uv - motion_vector;
    return history_uv;
}

vec2 FFX_DNSR_Reflections_GetHitPositionReprojection(ivec2 dispatch_thread_id, vec2 uv, float reflected_ray_length)
{
    float  z            = FFX_DNSR_Reflections_LoadDepth(dispatch_thread_id);
    vec3 view_space_ray = FFX_DNSR_Reflections_ScreenSpaceToViewSpace(vec3(uv, z));

    // We start out with reconstructing the ray length in view space.
    // This includes the portion from the camera to the reflecting surface as well as the portion from the surface to the hit position.
    float surface_depth = length(view_space_ray);
    float ray_length    = surface_depth + reflected_ray_length;

    // We then perform a parallax correction by shooting a ray
    // of the same length "straight through" the reflecting surface
    // and reprojecting the tip of that ray to the previous frame.
    view_space_ray /= surface_depth; // == normalize(view_space_ray)
    view_space_ray *= ray_length;
    vec3 world_hit_position =
        FFX_DNSR_Reflections_ViewSpaceToWorldSpace(vec4(view_space_ray, 1)); // This is the "fake" hit position if we would follow the ray straight through the surface.
    vec3 prev_hit_position = FFX_DNSR_Reflections_WorldSpaceToScreenSpacePrevious(world_hit_position);
    vec2 history_uv        = prev_hit_position.xy;
    return history_uv;
}

float FFX_DNSR_Reflections_GetDisocclusionFactor(vec3 normal, vec3 history_normal, float linear_depth, float history_linear_depth)
{
    float factor = 1.0
                        * exp(-abs(1.0 - max(0.0, dot(normal, history_normal))) * FFX_DNSR_REFLECTIONS_DISOCCLUSION_NORMAL_WEIGHT)
                        * exp(-abs(history_linear_depth - linear_depth) / linear_depth * FFX_DNSR_REFLECTIONS_DISOCCLUSION_DEPTH_WEIGHT);
    return factor;
}

struct FFX_DNSR_Reflections_Moments
{
    vec3 mean;
    vec3 variance;
};

FFX_DNSR_Reflections_Moments FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(ivec2 group_thread_id)
{
    FFX_DNSR_Reflections_Moments estimate;
    estimate.mean                = vec3(0);
    estimate.variance            = vec3(0);
    float accumulated_weight = 0;
    for (int j = -FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; j <= FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (int i = -FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; i <= FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            ivec2        new_idx  = group_thread_id + ivec2(i, j);
            vec3 radiance = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(new_idx).radiance;
            float weight  = FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(i) * FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(j);
            accumulated_weight  += weight;
            estimate.mean       += radiance * weight;
            estimate.variance   += radiance * radiance * weight;
        }
    }
    estimate.mean     /= accumulated_weight;
    estimate.variance /= accumulated_weight;

    estimate.variance = abs(estimate.variance - estimate.mean * estimate.mean);
    return estimate;
}

float dot2(vec3 a) { return dot(a, a); }

void FFX_DNSR_Reflections_PickReprojection(ivec2     dispatch_thread_id,  //
                                           ivec2     group_thread_id,     //
                                           uvec2     screen_size,         //
                                           float     roughness,           //
                                           float     ray_length,          //
                                           out float disocclusion_factor, //
                                           out vec2  reprojection_uv,     //
                                           out vec3  reprojection)
{

    FFX_DNSR_Reflections_Moments local_neighborhood = FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(group_thread_id);

    vec2 uv     = vec2(dispatch_thread_id.x + 0.5, dispatch_thread_id.y + 0.5) / screen_size;
    vec3 normal = FFX_DNSR_Reflections_LoadWorldSpaceNormal(dispatch_thread_id);
    vec3 history_normal;
    float history_linear_depth;

    {
        const vec2  motion_vector             = FFX_DNSR_Reflections_LoadMotionVector(dispatch_thread_id);
        const vec2  surface_reprojection_uv   = FFX_DNSR_Reflections_GetSurfaceReprojection(dispatch_thread_id, uv, motion_vector);
        const vec2  hit_reprojection_uv       = FFX_DNSR_Reflections_GetHitPositionReprojection(dispatch_thread_id, uv, ray_length);
        const vec3  surface_normal            = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(surface_reprojection_uv);
        const vec3  hit_normal                = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(hit_reprojection_uv);
        const vec3  surface_history           = FFX_DNSR_Reflections_SampleRadianceHistory(surface_reprojection_uv);
        const vec3  hit_history               = FFX_DNSR_Reflections_SampleRadianceHistory(hit_reprojection_uv);
        const float hit_normal_similarity     = dot(normalize(vec3(hit_normal)), normalize(vec3(normal)));
        const float surface_normal_similarity = dot(normalize(vec3(surface_normal)), normalize(vec3(normal)));
        const float hit_roughness             = FFX_DNSR_Reflections_SampleRoughnessHistory(hit_reprojection_uv);
        const float surface_roughness         = FFX_DNSR_Reflections_SampleRoughnessHistory(surface_reprojection_uv);

        // Choose reprojection uv based on similarity to the local neighborhood.
        if (hit_normal_similarity > FFX_DNSR_REFLECTIONS_REPROJECTION_NORMAL_SIMILARITY_THRESHOLD  // Candidate for mirror reflection parallax
            && hit_normal_similarity + 1.0e-3 > surface_normal_similarity                          //
            && abs(hit_roughness - roughness) < abs(surface_roughness - roughness) + 1.0e-3        //
        ) {
            history_normal                 = hit_normal;
            float hit_history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(hit_reprojection_uv);
            float hit_history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(hit_reprojection_uv, hit_history_depth);
            history_linear_depth           = hit_history_linear_depth;
            reprojection_uv                = hit_reprojection_uv;
            reprojection                   = hit_history;
        } else {
            // Reject surface reprojection based on simple distance
            if (dot2(surface_history - local_neighborhood.mean) <
                FFX_DNSR_REFLECTIONS_REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT * length(local_neighborhood.variance)) {
                history_normal                     = surface_normal;
                float surface_history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(surface_reprojection_uv);
                float surface_history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(surface_reprojection_uv, surface_history_depth);
                history_linear_depth               = surface_history_linear_depth;
                reprojection_uv                    = surface_reprojection_uv;
                reprojection                       = surface_history;
            } else {
                disocclusion_factor = 0.0;
                return;
            }
        }
    }
    float depth        = FFX_DNSR_Reflections_LoadDepth(dispatch_thread_id);
    float linear_depth = FFX_DNSR_Reflections_GetLinearDepth(uv, depth);
    // Determine disocclusion factor based on history
    disocclusion_factor = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);

    if (disocclusion_factor > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) // Early out, good enough
        return;

    // Try to find the closest sample in the vicinity if we are not convinced of a disocclusion
    if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) {
        vec2    closest_uv    = reprojection_uv;
        vec2    dudv          = 1.0 / vec2(screen_size);
        const int search_radius = 1;
        for (int y = -search_radius; y <= search_radius; y++) {
            for (int x = -search_radius; x <= search_radius; x++) {
                vec2      uv                   = reprojection_uv + vec2(x, y) * dudv;
                vec3 history_normal            = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(uv);
                float       history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(uv);
                float       history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(uv, history_depth);
                float       weight               = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
                if (weight > disocclusion_factor) {
                    disocclusion_factor = weight;
                    closest_uv          = uv;
                    reprojection_uv     = closest_uv;
                }
            }
        }
        reprojection = FFX_DNSR_Reflections_SampleRadianceHistory(reprojection_uv);
    }

    // Rare slow path - triggered only on the edges.
    // Try to get rid of potential leaks at bilinear interpolation level.
    if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) {
        // If we've got a discarded history, try to construct a better sample out of 2x2 interpolation neighborhood
        // Helps quite a bit on the edges in movement
        float       uvx                    = fract(float(screen_size.x) * reprojection_uv.x + 0.5);
        float       uvy                    = fract(float(screen_size.y) * reprojection_uv.y + 0.5);
        ivec2       reproject_texel_coords = ivec2(screen_size * reprojection_uv - 0.5);
        vec3 reprojection00                = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + ivec2(0, 0));
        vec3 reprojection10                = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + ivec2(1, 0));
        vec3 reprojection01                = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + ivec2(0, 1));
        vec3 reprojection11                = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + ivec2(1, 1));
        vec3 normal00                      = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + ivec2(0, 0));
        vec3 normal10                      = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + ivec2(1, 0));
        vec3 normal01                      = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + ivec2(0, 1));
        vec3 normal11                      = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + ivec2(1, 1));
        float       depth00                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + ivec2(0, 0)));
        float       depth10                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + ivec2(1, 0)));
        float       depth01                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + ivec2(0, 1)));
        float       depth11                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + ivec2(1, 1)));
        vec4 w                             = vec4(1.0);
        // Initialize with occlusion weights
        w.x = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal00, linear_depth, depth00) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
        w.y = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal10, linear_depth, depth10) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
        w.z = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal01, linear_depth, depth01) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
        w.w = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal11, linear_depth, depth11) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
        // And then mix in bilinear weights
        w.x           = w.x * (1.0 - uvx) * (1.0 - uvy);
        w.y           = w.y * (uvx) * (1.0 - uvy);
        w.z           = w.z * (1.0 - uvx) * (uvy);
        w.w           = w.w * uvx * uvy;
        float ws = max(w.x + w.y + w.z + w.w, 1.0e-3);
        // normalize
        w /= ws;

        vec3  history_normal;
        float history_linear_depth;
        reprojection         = reprojection00 * w.x + reprojection10 * w.y + reprojection01 * w.z + reprojection11 * w.w;
        history_linear_depth = depth00 * w.x + depth10 * w.y + depth01 * w.z + depth11 * w.w;
        history_normal       = normal00 * w.x + normal10 * w.y + normal01 * w.z + normal11 * w.w;
        disocclusion_factor  = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
    }
    disocclusion_factor = disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD ? 0.0 : disocclusion_factor;
}

void FFX_DNSR_Reflections_Reproject(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, float temporal_stability_factor, int max_samples)
{
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, ivec2(screen_size));
    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // Center threads in groupshared memory

    float       variance    = 1.0;
    float       num_samples = 0.0;
    float       roughness   = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    vec3        normal      = FFX_DNSR_Reflections_LoadWorldSpaceNormal(dispatch_thread_id);
    vec3        radiance    = FFX_DNSR_Reflections_LoadRadiance(dispatch_thread_id);
    const float ray_length  = FFX_DNSR_Reflections_LoadRayLength(dispatch_thread_id);

    if (FFX_DNSR_Reflections_IsGlossyReflection(roughness))
    {
        float disocclusion_factor;
        vec2      reprojection_uv;
        vec3   reprojection;
        FFX_DNSR_Reflections_PickReprojection(/*in*/ dispatch_thread_id,
                                              /* in */ group_thread_id,
                                              /* in */ screen_size,
                                              /* in */ roughness,
                                              /* in */ ray_length,
                                              /* out */ disocclusion_factor,
                                              /* out */ reprojection_uv,
                                              /* out */ reprojection);
        if (all(greaterThan(reprojection_uv, vec2(0.0))) && all(lessThan(reprojection_uv, vec2(1.0))))
        {
            float  prev_variance = FFX_DNSR_Reflections_SampleVarianceHistory(reprojection_uv);
            num_samples          = FFX_DNSR_Reflections_SampleNumSamplesHistory(reprojection_uv) * disocclusion_factor;
            float  s_max_samples = max(8.0, max_samples * FFX_DNSR_REFLECTIONS_SAMPLES_FOR_ROUGHNESS(roughness));
            num_samples          = min(s_max_samples, num_samples + 1);
            float  new_variance  = FFX_DNSR_Reflections_ComputeTemporalVariance(radiance.xyz, reprojection.xyz);
            if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD)
            {
                FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, vec3(0.0));
                FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, 1.0);
                FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, 1.0);
            }
            else
            {
                float variance_mix = mix(new_variance, prev_variance, 1.0 / num_samples);
                FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, reprojection);
                FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, variance_mix);
                FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, num_samples);
                // Mix in reprojection for radiance mip computation 
                radiance = mix(radiance, reprojection, vec3(0.3));
            }
        }
        else
        {
            FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, vec3(0.0));
            FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, 1.0);
            FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, 1.0);
        }
    }
    
    // Downsample 8x8 -> 1 radiance using groupshared memory
    // Initialize groupshared array for downsampling
    float weight = FFX_DNSR_Reflections_GetLuminanceWeight(radiance.xyz);
    radiance.xyz *= weight;
    if (any(greaterThanEqual(uvec2(dispatch_thread_id), screen_size)) || any(isinf(radiance)) || any(isnan(radiance)) || weight > 1.0e3)
    {
        radiance = vec3(0.0);
        weight   = 0.0;
    }

    group_thread_id -= 4; // Center threads in groupshared memory

    FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id, vec4(radiance.xyz, weight));
    groupMemoryBarrier();
    barrier();

    for (int i = 2; i <= 8; i = i * 2)
    {
        int ox = group_thread_id.x * i;
        int oy = group_thread_id.y * i;
        int ix = group_thread_id.x * i + i / 2;
        int iy = group_thread_id.y * i + i / 2;
        if (ix < 8 && iy < 8)
        {
            vec4 rad_weight00 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(ivec2(ox, oy));
            vec4 rad_weight10 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(ivec2(ox, iy));
            vec4 rad_weight01 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(ivec2(ix, oy));
            vec4 rad_weight11 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(ivec2(ix, iy));
            vec4 sum          = rad_weight00 + rad_weight01 + rad_weight10 + rad_weight11;
            FFX_DNSR_Reflections_StoreInGroupSharedMemory(ivec2(ox, oy), sum);
        }
        groupMemoryBarrier();
        barrier();
    }

    if (all(equal(group_thread_id, ivec2(0))))
    {
        vec4  sum          = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(ivec2(0, 0));
        float weight_acc   = max(sum.w, 1.0e-3);
        vec3  radiance_avg = sum.xyz / weight_acc;
        FFX_DNSR_Reflections_StoreAverageRadiance(dispatch_thread_id.xy / 8, radiance_avg);
    }
}

#endif // FFX_DNSR_REFLECTIONS_REPROJECT
