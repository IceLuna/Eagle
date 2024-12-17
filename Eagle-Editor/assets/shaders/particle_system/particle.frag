#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 1, binding = 0) uniform sampler2D g_Textures[];

// Inputs
layout(location = 0) in vec4 i_Color;
layout(location = 1) in vec2 i_UV;
layout(location = 2) flat in uint i_TextureIndex;
#ifdef EG_BLEND
layout(location = 3) flat in uint i_Additive;
#endif

// Outputs
layout(location = 0) out vec4 o_Color;

void main()
{
	vec4 color = i_Color;
	if (i_TextureIndex != 0)
		color *= texture(g_Textures[nonuniformEXT(i_TextureIndex)], i_UV);
#ifdef EG_BLEND
	color.rgb *= color.a;
	if (i_Additive != 0u)
		color.a = 0.f; // To not dim Dst Color
#endif
	o_Color = color;
}
