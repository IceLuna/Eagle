#ifndef EG_MSDF
#define EG_MSDF

#include "defines.h"

float MSDF_Median(vec3 msd)
{
    return max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
}

// How much of the pixel is covered by the glyph: 0 - outside, 1 - inside, 0.5 - on the glyph's edge.
// `msd` - texel of the MSDF atlas at `uv`
float MSDF_Coverage(vec3 msd, vec2 atlasSize, vec2 uv)
{
    const vec2 unitRange = vec2(EG_MSDF_PIXEL_RANGE) / atlasSize;
    const vec2 screenTexSize = vec2(1.0) / fwidth(uv);
    const float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
    const float screenPxDistance = screenPxRange * (MSDF_Median(msd) - 0.5);
    return clamp(screenPxDistance + 0.5, 0.0, 1.0);
}

#define MSDF_COVERAGE(atlas, uv) MSDF_Coverage(texture(atlas, uv).rgb, vec2(textureSize(atlas, 0)), uv)

#endif
