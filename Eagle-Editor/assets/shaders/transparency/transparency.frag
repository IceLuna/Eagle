// Loop32 was used from https://github.com/nvpro-samples/vk_order_independent_transparency
// Minor modification were made to the color & composite algorithms to store HDR colors

#include "utils.h"

////////////////////////////////////////////////////////////////////////////////
// Depth sorting pass                                                         //
////////////////////////////////////////////////////////////////////////////////
#ifdef EG_DEPTH_PASS

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    layout(offset = 64) ivec2 g_Size;
};

#extension GL_ARB_post_depth_coverage : enable
layout(post_depth_coverage) in;

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
void DoBlendPacked(inout vec4 color, vec4 fragmentColor)
{
    // Convert from unpremultiplied sRGB to premultiplied alpha
    fragmentColor.rgb *= fragmentColor.a;
    DoBlend(color, fragmentColor);
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
    const int listPos2 = listPos + viewSize * EG_OIT_LAYERS * 2;
    listPos += viewSize * EG_OIT_LAYERS;

    for(int i = 0; i < fragments; i++)
    {
        uint fragment1 = imageLoad(imgAbuffer, listPos + i * viewSize).r;
        uint fragment2 = imageLoad(imgAbuffer, listPos2 + i * viewSize).r;
        vec4 unpackedColor = vec4(unpackHalf2x16(fragment1), unpackHalf2x16(fragment2));
        DoBlendPacked(color, unpackedColor);
    }

    outColor = color;
}

#endif // #ifdef EG_COMPOSITE_PASS
