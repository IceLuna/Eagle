#ifndef EG_TEXT_EFFECTS
#define EG_TEXT_EFFECTS

#include "text/msdf.h"

// Sizes are in layout units (font sizes)
// Note: keep in sync with `TextEffectsData` in `RenderText2DTask.h`
struct TextEffects
{
    vec4 OutlineColor;
    vec4 GlowColor;
    vec4 ShadowColor;
    float OutlineWidth;
    float GlowSize;
    float ShadowSoftness;
    float DistanceRange;
};

// Outlines and glows use the true distance field (alpha of the MTSDF atlas), so their outer corners are rounded.
// Distance to the outer edge of the outline in layout units. Positive inside
float TextEffects_Distance(vec4 mtsdf, TextEffects effects)
{
    return (mtsdf.a - 0.5) * effects.DistanceRange + effects.OutlineWidth;
}

// Glyph with its outline and glow. Not premultiplied
vec4 TextEffects_Shade(vec4 mtsdf, vec2 atlasSize, vec2 uv, vec3 color, TextEffects effects)
{
    const float screenPxRange = MSDF_ScreenPxRange(atlasSize, uv);
    const float fill = clamp(screenPxRange * (MSDF_Median(mtsdf.rgb) - 0.5) + 0.5, 0.0, 1.0);
    if (effects.OutlineWidth <= 0.0 && effects.GlowSize <= 0.0)
        return vec4(color, fill);

    const float distance = TextEffects_Distance(mtsdf, effects);
    const float pxPerUnit = screenPxRange / effects.DistanceRange;

    // Premultiplied, composited back to front: glow, outline, glyph
    vec4 result = vec4(0.0);
    if (effects.GlowSize > 0.0)
    {
        const float glow = effects.GlowColor.a * (1.0 - smoothstep(0.0, effects.GlowSize, -distance));
        result = vec4(effects.GlowColor.rgb * glow, glow);
    }
    if (effects.OutlineWidth > 0.0)
    {
        const float outline = effects.OutlineColor.a * clamp(distance * pxPerUnit + 0.5, 0.0, 1.0);
        result = vec4(effects.OutlineColor.rgb * outline, outline) + result * (1.0 - outline);
    }
    result = vec4(color * fill, fill) + result * (1.0 - fill);

    return (result.a > 0.0) ? vec4(result.rgb / result.a, result.a) : vec4(0.0);
}

// Opacity of the shadow of the glyph (and its outline)
float TextEffects_Shadow(vec4 mtsdf, vec2 atlasSize, vec2 uv, TextEffects effects)
{
    const float pxPerUnit = MSDF_ScreenPxRange(atlasSize, uv) / effects.DistanceRange;
    const float softness = max(effects.ShadowSoftness, 1.0 / pxPerUnit); // At least one pixel, for anti-aliasing
    return effects.ShadowColor.a * clamp(TextEffects_Distance(mtsdf, effects) / softness + 0.5, 0.0, 1.0);
}

#endif
