layout(location = 0) in vec4  a_AlbedoRoughness;
layout(location = 1) in vec4  a_EmissiveMetallness;
layout(location = 2) in vec2  a_Position;
layout(location = 3) in vec2  a_TexCoords;
layout(location = 4) in int   a_EntityID;
layout(location = 5) in uint  a_AtlasIndex;
layout(location = 6) in float a_AO;
layout(location = 7) in float a_Opacity;
layout(location = 8) in float a_OpacityMask;
layout(location = 9) in uint a_TransformIndex;

const vec3 s_Normal = vec3(0.0f, 0.0f, 1.0f);
