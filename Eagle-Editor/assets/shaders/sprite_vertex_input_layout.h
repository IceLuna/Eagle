layout(location = 0) in vec3  a_WorldPosition;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec3  a_WorldTangent;
layout(location = 3) in vec3  a_WorldBitangent;
layout(location = 4) in vec3  a_WorldNormal;
layout(location = 5) in vec2  a_TexCoords;
layout(location = 6) in int   a_EntityID;

// Material data
layout(location = 7)  in vec4  a_TintColor;
layout(location = 8)  in vec3  a_EmissionIntensity;
layout(location = 9)  in float a_TilingFactor;
layout(location = 10) in uint  a_PackedTextureIndices;
layout(location = 11) in uint  a_PackedTextureIndices2;
