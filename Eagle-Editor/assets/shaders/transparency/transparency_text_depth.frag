// Loop32 was used from https://github.com/nvpro-samples/vk_order_independent_transparency
// Minor modification were made to the color & composite algorithms to store HDR colors

#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////
// Depth sorting pass                                                         //
////////////////////////////////////////////////////////////////////////////////

// Input
layout(location = 0) in vec2      i_TexCoords;
layout(location = 1) flat in uint i_AtlasIndex;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    layout(offset = 64) ivec2 g_Size;
};

#extension GL_ARB_post_depth_coverage : enable
layout(post_depth_coverage) in;

layout(binding = 1, r32ui) uniform coherent uimageBuffer imgAbuffer;

layout(set = 1, binding = 0) uniform sampler2D g_FontAtlases[EG_MAX_TEXTURES];

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange()
{
	const float pxRange = 2.0f;
    const vec2 unitRange = vec2(pxRange) / vec2(textureSize(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(i_TexCoords);
    return max(0.5f * dot(unitRange, screenTexSize), 1.0);
}

void main()
{
    const vec3 msd = texture(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords).rgb;
    const float sd = median(msd.r, msd.g, msd.b);
    const float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
    const float fadeoutOpacity = clamp(screenPxDistance + 0.5f, 0.f, 1.f);

	if (IS_ZERO(fadeoutOpacity))
    {
		discard;
        return;
    }

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
