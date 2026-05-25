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

#ifndef FFX_DNSR_REFLECTIONS_RESOLVE_TEMPORAL
#define FFX_DNSR_REFLECTIONS_RESOLVE_TEMPORAL

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
    uvec2  packed_radiance = uvec2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    vec3 unpacked_radiance = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance).xyz;

    FFX_DNSR_Reflections_NeighborhoodSample smpl;
    smpl.radiance = unpacked_radiance;
    return smpl;
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
    for (int j = -FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; j <= FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; ++j)
    {
        for (int i = -FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; i <= FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; ++i)
        {
            ivec2     new_idx  = group_thread_id + ivec2(i, j);
            vec3   radiance    = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(new_idx).radiance;
            float weight       = FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(float(i)) * FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(float(j));
            accumulated_weight += weight;
            estimate.mean      += radiance * weight;
            estimate.variance  += radiance * radiance * weight;
        }
    }
    estimate.mean     /= accumulated_weight;
    estimate.variance /= accumulated_weight;

    estimate.variance = abs(estimate.variance - estimate.mean * estimate.mean);
    return estimate;
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(ivec2 group_thread_id, vec3 radiance)
{
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x] = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x] = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
}

void FFX_DNSR_Reflections_LoadNeighborhood(ivec2 pixel_coordinate, out vec3 radiance)
{
    radiance = FFX_DNSR_Reflections_LoadRadiance(pixel_coordinate);
}

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size)
{
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    vec3 radiance[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (int i = 0; i < 4; ++i)
    {
        FFX_DNSR_Reflections_LoadNeighborhood(dispatch_thread_id + offset[i], radiance[i]);
    }

    // Then move all registers to groupshared memory
    for (int j = 0; j < 4; ++j)
    {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j]);
    }
}

void FFX_DNSR_Reflections_ResolveTemporal(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, vec2 inv_screen_size, float history_clip_weight)
{
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, ivec2(screen_size));
    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // Center threads in groupshared memory

    FFX_DNSR_Reflections_NeighborhoodSample center       = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(group_thread_id);
    vec3                                    new_signal   = center.radiance;
    float                                   roughness    = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    float                                   new_variance = FFX_DNSR_Reflections_LoadVariance(dispatch_thread_id);
    
    if (FFX_DNSR_Reflections_IsGlossyReflection(float(roughness)))
    {
        float  num_samples  = FFX_DNSR_Reflections_LoadNumSamples(dispatch_thread_id);
        vec2   uv8          = (vec2(dispatch_thread_id.xy) + (0.5).xx) / FFX_DNSR_Reflections_RoundUp8(screen_size);
        vec3   avg_radiance = FFX_DNSR_Reflections_SampleAverageRadiance(uv8);

        vec3                      old_signal         = FFX_DNSR_Reflections_LoadRadianceReprojected(dispatch_thread_id);
        FFX_DNSR_Reflections_Moments local_neighborhood = FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(group_thread_id);
        // Clip history based on the curren local statistics
        vec3         color_std          = (sqrt(local_neighborhood.variance.xyz) + length(local_neighborhood.mean.xyz - avg_radiance)) * (history_clip_weight * 1.4);
                     local_neighborhood.mean.xyz = mix(local_neighborhood.mean.xyz, avg_radiance, vec3(0.2));
        vec3         radiance_min       = local_neighborhood.mean.xyz - color_std;
        vec3         radiance_max       = local_neighborhood.mean.xyz + color_std;
        vec3         clipped_old_signal = FFX_DNSR_Reflections_ClipAABB(radiance_min, radiance_max, old_signal.xyz);
        float        accumulation_speed = 1.0 / max(num_samples, 1.0);
        float        weight             = (1.0 - accumulation_speed);
        // Blend with average for small sample count
        new_signal.xyz                                  = mix(new_signal.xyz, avg_radiance, 1.0 / max(num_samples + 1.0f, 1.0));
        // Clip outliers
        {
            vec3 radiance_min = avg_radiance.xyz - color_std * 1.0;
            vec3 radiance_max = avg_radiance.xyz + color_std * 1.0;
            new_signal.xyz       = FFX_DNSR_Reflections_ClipAABB(radiance_min, radiance_max, new_signal.xyz);
        }
        // Blend with history
        new_signal   = mix(new_signal, clipped_old_signal, weight);
        new_variance = mix(FFX_DNSR_Reflections_ComputeTemporalVariance(new_signal.xyz, clipped_old_signal.xyz), new_variance, weight);
        if (any(isinf(vec3(new_signal))) || any(isnan(vec3(new_signal))) || any(isinf(vec3(new_variance))) || any(isnan(vec3(new_variance))))
        {
            new_signal   = vec3(0.0);
            new_variance = 0.0;
        }

    }
    FFX_DNSR_Reflections_StoreTemporalAccumulation(dispatch_thread_id, new_signal, new_variance);
}

#endif // FFX_DNSR_REFLECTIONS_RESOLVE_TEMPORAL
