#ifndef EG_PIPELINE_LAYOUT
#define EG_PIPELINE_LAYOUT

#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "common_structures.h"
#include "utils.h"

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_TEXTURES) uniform sampler2D g_Textures[EG_MAX_TEXTURES];

vec4 ReadTexture(uint index, vec2 uv)
{
	return texture(g_Textures[nonuniformEXT(index)], uv);
}

#endif
