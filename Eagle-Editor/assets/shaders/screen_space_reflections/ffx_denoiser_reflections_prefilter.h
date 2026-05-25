
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

#ifndef FFX_DNSR_REFLECTIONS_PREFILTER
#define FFX_DNSR_REFLECTIONS_PREFILTER

#include "screen_space_reflections/ffx_denoiser_reflections_common.h"

shared uint  g_ffx_dnsr_shared_0[16][16];
shared uint  g_ffx_dnsr_shared_1[16][16];
shared uint  g_ffx_dnsr_shared_2[16][16];
shared uint  g_ffx_dnsr_shared_3[16][16];
shared float g_ffx_dnsr_shared_depth[16][16];

struct FFX_DNSR_Reflections_NeighborhoodSample
{
    vec3 radiance;
    float variance;
    vec3 normal;
    float depth;
};

FFX_DNSR_Reflections_NeighborhoodSample FFX_DNSR_Reflections_LoadFromGroupSharedMemory(ivec2 idx)
{
    uvec2   packed_radiance        = uvec2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    vec4 unpacked_radiance         = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);
    uvec2   packed_normal_variance = uvec2(g_ffx_dnsr_shared_2[idx.y][idx.x], g_ffx_dnsr_shared_3[idx.y][idx.x]);
    vec4 unpacked_normal_variance  = FFX_DNSR_Reflections_UnpackFloat16_4(packed_normal_variance);

    FFX_DNSR_Reflections_NeighborhoodSample smpl;
    smpl.radiance = unpacked_radiance.xyz;
    smpl.normal   = unpacked_normal_variance.xyz;
    smpl.variance = unpacked_normal_variance.w;
    smpl.depth    = g_ffx_dnsr_shared_depth[idx.y][idx.x];
    return smpl;
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(ivec2 group_thread_id, vec3 radiance, float variance, vec3 normal, float depth)
{
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
    g_ffx_dnsr_shared_2[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(normal.xy);
    g_ffx_dnsr_shared_3[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(vec2(normal.z, variance));
    g_ffx_dnsr_shared_depth[group_thread_id.y][group_thread_id.x] = depth;
}

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size)
{
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    vec3   radiance[4];
    float  variance[4];
    vec3   normal[4];
    float  depth[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (int i = 0; i < 4; ++i)
    {
        FFX_DNSR_Reflections_LoadNeighborhood(dispatch_thread_id + offset[i], radiance[i], variance[i], normal[i], depth[i], screen_size);
    }

    // Then move all registers to groupshared memory
    for (int j = 0; j < 4; ++j)
    {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j], variance[j], normal[j], depth[j]); // X
    }
}

float FFX_DNSR_Reflections_GetEdgeStoppingNormalWeight(vec3 normal_p, vec3 normal_q)
{
    return pow(max(dot(normal_p, normal_q), 0.0), FFX_DNSR_REFLECTIONS_PREFILTER_NORMAL_SIGMA);
}

float FFX_DNSR_Reflections_GetEdgeStoppingDepthWeight(float center_depth, float neighbor_depth)
{
    return exp(-abs(center_depth - neighbor_depth) * center_depth * FFX_DNSR_REFLECTIONS_PREFILTER_DEPTH_SIGMA);
}

float FFX_DNSR_Reflections_GetRadianceWeight(vec3 center_radiance, vec3 neighbor_radiance, float variance)
{
    return max(exp(-(FFX_DNSR_REFLECTIONS_RADIANCE_WEIGHT_BIAS + variance * FFX_DNSR_REFLECTIONS_RADIANCE_WEIGHT_VARIANCE_K)
                    * length(center_radiance - neighbor_radiance.xyz))
            , 1.0e-2);
}

void FFX_DNSR_Reflections_Resolve(ivec2 group_thread_id, vec3 avg_radiance, FFX_DNSR_Reflections_NeighborhoodSample center,
                                  out vec3 resolved_radiance, out float resolved_variance)
{
    // Initial weight is important to remove fireflies.
    // That removes quite a bit of energy but makes everything much more stable.
    float accumulated_weight   = FFX_DNSR_Reflections_GetRadianceWeight(avg_radiance, center.radiance.xyz, center.variance);
    vec3  accumulated_radiance = center.radiance.xyz * accumulated_weight;
    float accumulated_variance = center.variance * accumulated_weight * accumulated_weight;
    // First 15 numbers of Halton(2,3) streteched to [-3,3]. Skipping the center, as we already have that in center_radiance and center_variance.
    const uint sample_count      = 15;
    const ivec2 sample_offsets[] = {ivec2(0, 1),  ivec2(-2, 1),  ivec2(2, -3), ivec2(-3, 0),  ivec2(1, 2), ivec2(-1, -2), ivec2(3, 0), ivec2(-3, 3),
                                    ivec2(0, -3), ivec2(-1, -1), ivec2(2, 1),  ivec2(-2, -2), ivec2(1, 0), ivec2(0, 2),   ivec2(3, -1)};
    float variance_weight = max(FFX_DNSR_REFLECTIONS_PREFILTER_VARIANCE_BIAS,
                                     1.0 - exp(-(center.variance * FFX_DNSR_REFLECTIONS_PREFILTER_VARIANCE_WEIGHT))
                                    );
    for (int i = 0; i < sample_count; ++i)
    {
        ivec2                                   new_idx  = group_thread_id + sample_offsets[i];
        FFX_DNSR_Reflections_NeighborhoodSample neighbor = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(new_idx);

        float weight = 1.0;
        weight *= FFX_DNSR_Reflections_GetEdgeStoppingNormalWeight(center.normal, neighbor.normal);
        weight *= FFX_DNSR_Reflections_GetEdgeStoppingDepthWeight(center.depth, neighbor.depth);
        weight *= FFX_DNSR_Reflections_GetRadianceWeight(avg_radiance, neighbor.radiance.xyz, center.variance);
        weight *= variance_weight;

        // Accumulate all contributions.
        accumulated_weight += weight;
        accumulated_radiance += weight * neighbor.radiance.xyz;
        accumulated_variance += weight * weight * neighbor.variance;
    }

    accumulated_radiance /= accumulated_weight;
    accumulated_variance /= (accumulated_weight * accumulated_weight);
    resolved_radiance = accumulated_radiance;
    resolved_variance = accumulated_variance;
}

void FFX_DNSR_Reflections_Prefilter(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size)
{
    float center_roughness = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, ivec2(screen_size));
    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // Center threads in groupshared memory

    FFX_DNSR_Reflections_NeighborhoodSample center = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(group_thread_id);

    vec3  resolved_radiance = center.radiance;
    float resolved_variance = center.variance;

    // Check if we have to denoise or if a simple copy is enough
    bool needs_denoiser = center.variance > 0.0 && FFX_DNSR_Reflections_IsGlossyReflection(center_roughness) && !FFX_DNSR_Reflections_IsMirrorReflection(center_roughness);
    if (needs_denoiser)
    {
        vec2 uv8          = (vec2(dispatch_thread_id.xy) + (0.5).xx) / FFX_DNSR_Reflections_RoundUp8(screen_size);
        vec3 avg_radiance = FFX_DNSR_Reflections_SampleAverageRadiance(uv8);
        FFX_DNSR_Reflections_Resolve(group_thread_id, avg_radiance, center, resolved_radiance, resolved_variance);
    }

    FFX_DNSR_Reflections_StorePrefilteredReflections(dispatch_thread_id, resolved_radiance, resolved_variance);
}

#endif // FFX_DNSR_REFLECTIONS_PREFILTER
