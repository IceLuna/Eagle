#ifndef EG_UTILS
#define EG_UTILS

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

#endif
