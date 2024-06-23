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

#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

const float g_roughness_sigma_min = 0.01f;
const float g_roughness_sigma_max = 0.02f;
const float g_depth_sigma = 0.02f;

layout(binding = 0) uniform SSSRData
{
    mat4 g_View;
    mat4 g_Proj;
    mat4 g_InvProj;
    mat4 g_InvView;
    mat4 g_InvViewProj;
    mat4 g_PrevViewProj;
    float g_RoughnessThreshold;
};

//=== Common functions of the SssrSample ===

uint PackFloat16(f16vec2 v)
{
    uvec2 p = uvec2(halfBitsToUint16(v.x), halfBitsToUint16(v.y));
    return p.x | (p.y << 16);
}

f16vec2 UnpackFloat16(uint a)
{
    u16vec2 temp = u16vec2(a & 0xFFFF, a >> 16);
    return f16vec2(uint16BitsToHalf(temp.x), uint16BitsToHalf(temp.y));
}

uint PackRayCoords(uvec2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal)
{
    uint ray_x_15bit = ray_coord.x & 32767; // 0b111111111111111;
    uint ray_y_14bit = ray_coord.y & 16383; // 0b11111111111111;
    uint copy_horizontal_1bit = copy_horizontal ? 1 : 0;
    uint copy_vertical_1bit = copy_vertical ? 1 : 0;
    uint copy_diagonal_1bit = copy_diagonal ? 1 : 0;

    uint packed = (copy_diagonal_1bit << 31) | (copy_vertical_1bit << 30) | (copy_horizontal_1bit << 29) | (ray_y_14bit << 15) | (ray_x_15bit << 0);
    return packed;
}

void UnpackRayCoords(uint packed, out ivec2 ray_coord, out bool copy_horizontal, out bool copy_vertical, out bool copy_diagonal)
{
    ray_coord.x = int((packed >> 0) & 32767); // 0b111111111111111;
    ray_coord.y = int((packed >> 15) & 16383); // 0b11111111111111;
    copy_horizontal = bool((packed >> 29) & 1);
    copy_vertical   = bool((packed >> 30) & 1);
    copy_diagonal   = bool((packed >> 31) & 1);
}

// Transforms origin to uv space
// Mat must be able to transform origin from its current space into clip space.
vec3 ProjectPosition(vec3 origin, mat4 mat)
{
    vec4 projected = mat * vec4(origin, 1);
    projected.xyz /= projected.w;
    projected.xy = 0.5 * projected.xy + 0.5;
    return projected.xyz;
}

// Origin and direction must be in the same space and mat must be able to transform from that space into clip space.
vec3 ProjectDirection(vec3 origin, vec3 direction, vec3 screen_space_origin, mat4 mat)
{
    vec3 offsetted = ProjectPosition(origin + direction, mat);
    return offsetted - screen_space_origin;
}

// Mat must be able to transform origin from texture space to a linear space.
vec3 InvProjectPosition(vec3 coord, mat4 mat)
{
    coord.xy = 2 * coord.xy - 1;
    vec4 projected = mat * vec4(coord, 1);
    projected.xyz /= projected.w;
    return projected.xyz;
}

//=== FFX_DNSR_Reflections_ override functions ===

bool FFX_DNSR_Reflections_IsGlossyReflection(float roughness)
{
    return roughness < (g_RoughnessThreshold * g_RoughnessThreshold);
}

bool FFX_DNSR_Reflections_IsMirrorReflection(float roughness)
{
    return roughness < EG_SQUARE(EG_MIN_ROUGHNESS + 0.01f);
}

vec3 FFX_DNSR_Reflections_ScreenSpaceToViewSpace(vec3 screen_uv_coord)
{
    return InvProjectPosition(screen_uv_coord, g_InvProj);
}

vec3 FFX_DNSR_Reflections_ViewSpaceToWorldSpace(vec4 view_space_coord)
{
    return vec3(g_InvView * view_space_coord);
}

vec3 FFX_DNSR_Reflections_WorldSpaceToScreenSpacePrevious(vec3 world_space_pos)
{
    return ProjectPosition(world_space_pos, g_PrevViewProj);
}

float FFX_DNSR_Reflections_GetLinearDepth(vec2 uv, float depth)
{
    const vec3 view_space_pos = InvProjectPosition(vec3(uv, depth), g_InvProj);
    return abs(view_space_pos.z);
}

uint FFX_DNSR_Reflections_RoundedDivide(uint value, uint divisor)
{
    return (value + divisor - 1) / divisor;
}

uint FFX_DNSR_Reflections_GetTileMetaDataIndex(uvec2 pixel_pos, uint screen_width)
{
    uvec2 tile_index = uvec2(pixel_pos.x / 8, pixel_pos.y / 8);
    uint flattened = tile_index.y * FFX_DNSR_Reflections_RoundedDivide(screen_width, 8) + tile_index.x;
    return flattened;
}
