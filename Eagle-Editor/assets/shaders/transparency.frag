#ifndef EG_COMPOSITE_PASS

#include "pipeline_layout.h"
#include "utils.h"

// Input
layout(location = 0) in vec3 i_Normal;
layout(location = 1) in vec2 i_TexCoords;
layout(location = 2) flat in uint i_MaterialIndex;
layout(location = 3) flat in int i_EntityID;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    layout(offset = 64) ivec2 g_Size;
};

#extension GL_ARB_post_depth_coverage : enable
layout(post_depth_coverage) in;

#endif // #ifndef EG_COMPOSITE_PASS

////////////////////////////////////////////////////////////////////////////////
// Depth sorting pass                                                         //
////////////////////////////////////////////////////////////////////////////////
#ifdef EG_DEPTH_PASS

layout(binding = 1, r32ui) uniform coherent uimageBuffer imgAbuffer;

void main()
{
    // The number of pixels in the image
    const int viewSize = g_Size.x * g_Size.y;
    const ivec2 coord = ivec2(gl_FragCoord.xy);
    const int listPos = coord.y * g_Size.x + coord.x;

    // Insert the floating-point depth (reinterpreted as a uint) into the list of depths
    uint zcur = floatBitsToUint(gl_FragCoord.z);
    int i = 0;  // Current position in the array

    // Do some early tests to minimize the amount of insertion-sorting work we
    // have to do.
    // If the fragment is further away than the last depth fragment, skip it:
    uint pretest = imageLoad(imgAbuffer, listPos + (EG_OIT_LAYERS - 1) * viewSize).x;
    if(zcur > pretest)
        return;

    // Check to see if the fragment can be inserted in the latter half of the
    // depth array:
    pretest = imageLoad(imgAbuffer, listPos + (EG_OIT_LAYERS / 2) * viewSize).x;
    if(zcur > pretest)
        i = (EG_OIT_LAYERS / 2);

    // Try to insert zcur in the place of the first element of the array that
    // is greater than or equal to it. In the former case, shift all of the
    // remaining elements in the array down.
    for(; i < EG_OIT_LAYERS; i++)
    {
        const uint ztest = imageAtomicMin(imgAbuffer, listPos + i * viewSize, zcur);
        if(ztest == 0xFFFFFFFFu || ztest == zcur)
        {
            // In the former case, we just inserted zcur into an empty space in the
            // array. In the latter case, we found a depth value that exactly matched.
            break;
        }
        zcur = max(ztest, zcur);
    }

    // Note that this line is necessary, since otherwise we'll get a warning from
    // the validation layer saying that undefined values will be written.
    // TODO: See if we can remove this
    outColor = vec4(0);
}

#endif // #ifdef EG_DEPTH_PASS

////////////////////////////////////////////////////////////////////////////////
// Color                                                                      //
////////////////////////////////////////////////////////////////////////////////
#ifdef EG_COLOR_PASS

// For each fragment, we look up its depth in the sorted array of depths.
// If we find a match, we write the fragment's color into the corresponding
// place in an array of colors. Otherwise, we tail blend it if enabled.

// Converts an unpremultiplied scalar from linear space to sRGB. Note that
// this does not match the standard behavior outside [0,1].
float unPremultLinearToSRGB(float c)
{
    if(c < 0.0031308f)
        return c * 12.92f;
    else
        return (pow(c, 1.0f / 2.4f) * 1.055f) - 0.055f;
}

// Converts an unpremultiplied RGB color from linear space to sRGB. Note that
// this does not match the standard behavior outside [0,1].
vec4 unPremultLinearToSRGB(vec4 c)
{
    c.r = unPremultLinearToSRGB(c.r);
    c.g = unPremultLinearToSRGB(c.g);
    c.b = unPremultLinearToSRGB(c.b);
    return c;
}

#include "material_pipeline_layout.h"

layout(binding = EG_BINDING_MAX + 1, r32ui) uniform coherent uimageBuffer imgAbuffer;

void main()
{
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex);
    const vec2 uv = i_TexCoords * material.TilingFactor;
    const vec3 albedo = ReadTexture(material.AlbedoTextureIndex, uv).rgb;
    const float opacity = 0.5f;//ReadTexture(material.OpacityTextureIndex, uv).r;

    // Get the unpremultiplied linear-space RGBA color of this pixel
    vec4 color = vec4(albedo, opacity);
    // Convert to unpremultiplied sRGB for 8-bit storage
    const vec4 sRGBColor = unPremultLinearToSRGB(color);

    // Compute base index in the A-buffer
    const int viewSize = g_Size.x * g_Size.y;
    ivec2 coord = ivec2(gl_FragCoord.xy);
    const int listPos = coord.y * g_Size.x + coord.x;

    const uint zcur = floatBitsToUint(gl_FragCoord.z);

    // If this fragment was behind the frontmost EG_OIT_LAYERS fragments, it didn't
    // make it in, so tail blend it:
    if(imageLoad(imgAbuffer, listPos + (EG_OIT_LAYERS - 1) * viewSize).x < zcur)
    {
        outColor = vec4(color.rgb * color.a, color.a); // TAILBLEND
        return;
    }

    // Use binary search to determine which index this depth value corresponds to
    // At each step, we know that it'll be in the closed interval [start, end].
    int start = 0;
    int end = (EG_OIT_LAYERS - 1);
    uint ztest;
    while(start < end)
    {
        int mid = (start + end) / 2;
        ztest = imageLoad(imgAbuffer, listPos + mid * viewSize).x;
        if(ztest < zcur)
            start = mid + 1;  // in [mid + 1, end]
        else
            end = mid;  // in [start, mid]
    }

    // We now have start == end. Insert the packed color into the A-buffer at
    // this index.
    imageStore(imgAbuffer, listPos + (EG_OIT_LAYERS + start) * viewSize, uvec4(packUnorm4x8(sRGBColor)));

    // Inserted, so make this color transparent:
    outColor = vec4(0);
}

#endif // #ifdef EG_COLOR_PASS

////////////////////////////////////////////////////////////////////////////////
// Composite                                                                  //
////////////////////////////////////////////////////////////////////////////////
#ifdef EG_COMPOSITE_PASS

// Gets the colors in the second part of the A-buffer (which are already sorted
// front to back) and bends them together.

layout(binding = 0, r32ui) uniform restrict readonly uimageBuffer imgAbuffer;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    ivec2 g_Size;
};

// Converts an unpremultiplied scalar from sRGB to linear space. Note that
// this does not match the standard behavior outside [0,1].
float unPremultSRGBToLinear(float c)
{
    if(c < 0.04045f)
        return c / 12.92f;
    else
        return pow((c + 0.055f) / 1.055f, 2.4f);
}

// Converts an unpremultiplied RGBA color from sRGB to linear space. Note that
// this does not match the standard behavior outside [0,1].
vec4 unPremultSRGBToLinear(vec4 c)
{
    c.r = unPremultSRGBToLinear(c.r);
    c.g = unPremultSRGBToLinear(c.g);
    c.b = unPremultSRGBToLinear(c.b);
    return c;
}

// Sets color to the result of blending color over baseColor.
// Color and baseColor are both premultiplied colors.
void DoBlend(inout vec4 color, vec4 baseColor)
{
    color.rgb += (1 - color.a) * baseColor.rgb;
    color.a += (1 - color.a) * baseColor.a;
}

// Sets color to the result of blending color over fragment.
// Color and fragment are both premultiplied colors; fragment
// is an rgba8 sRGB unpremultiplied color packed in a 32-bit uint.
void DoBlendPacked(inout vec4 color, uint fragment)
{
    vec4 unpackedColor = unpackUnorm4x8(fragment);
    // Convert from unpremultiplied sRGB to premultiplied alpha
    unpackedColor = unPremultSRGBToLinear(unpackedColor);
    unpackedColor.rgb *= unpackedColor.a;
    DoBlend(color, unpackedColor);
}

void main()
{
    vec4 color = vec4(0);

    const int viewSize = g_Size.x * g_Size.y;
    const ivec2 coord = ivec2(gl_FragCoord.xy);
    int listPos  = coord.y * g_Size.x + coord.x;

    // Count the number of fragments for this pixel
    int fragments = 0;
    for(int i = 0; i < EG_OIT_LAYERS; i++)
    {
        const uint ztest = imageLoad(imgAbuffer, listPos + i * viewSize).r;
        if(ztest != 0xFFFFFFFFu)
            fragments++;
        else
            break;
    }

    // Jump ahead to the color portion of the A-buffer
    listPos += viewSize * EG_OIT_LAYERS;

    for(int i = 0; i < fragments; i++)
        DoBlendPacked(color, imageLoad(imgAbuffer, listPos + i * viewSize).r);

    outColor = color;
}

#endif // #ifdef EG_COMPOSITE_PASS
