layout(location = 0) in vec3 a_ModelViewPos;
layout(location = 1) in vec3 a_PrevModelViewPos;
layout(location = 2) in uint a_TextureIndex;
layout(location = 3) in int  a_EntityID;

const vec2 s_TexCoords[4] = {
	vec2(0.f, 1.f),
	vec2(1.f, 1.f),
	vec2(1.f, 0.f),
	vec2(0.f, 0.f)
};
