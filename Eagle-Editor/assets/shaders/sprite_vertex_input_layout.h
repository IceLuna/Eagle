layout(location = 0) in vec2 a_TexCoords;
layout(location = 1) in int  a_EntityID;
layout(location = 2) in uint a_MaterialIndex;
layout(location = 3) in uint a_TransformIndex;

const vec3 s_Normal = vec3(0.0f, 0.0f, 1.0f);

const float s_QuadPosition = 0.5f;
const vec3 s_QuadVertexPosition[4] = {
	vec3(-0.5f, -0.5f, 0.0f),
	vec3(0.5f, -0.5f, 0.0f),
	vec3(0.5f,  0.5f, 0.0f),
	vec3(-0.5f,  0.5f, 0.0f)
};

const vec2 s_TexCoords[4] = {
	vec2(0.0f, 1.0f),
	vec2(1.f, 1.f),
	vec2(1.f, 0.f),
	vec2(0.f, 0.f)
};

const vec3 Edge1 = s_QuadVertexPosition[1] - s_QuadVertexPosition[0];
const vec3 Edge2 = s_QuadVertexPosition[2] - s_QuadVertexPosition[0];
const vec2 DeltaUV1 = s_TexCoords[1] - s_TexCoords[0];
const vec2 DeltaUV2 = s_TexCoords[2] - s_TexCoords[0];
const float f = 1.0f / (DeltaUV1.x * DeltaUV2.y - DeltaUV2.x * DeltaUV1.y);


const vec3 s_Tangent = vec3(
	f * (DeltaUV2.y * Edge1.x - DeltaUV1.y * Edge2.x),
	f * (DeltaUV2.y * Edge1.y - DeltaUV1.y * Edge2.y),
	f * (DeltaUV2.y * Edge1.z - DeltaUV1.y * Edge2.z)
);

const vec3 s_Bitangent = vec3(
	f * (-DeltaUV2.x * Edge1.x + DeltaUV1.x * Edge2.x),
	f * (-DeltaUV2.x * Edge1.y + DeltaUV1.x * Edge2.y),
	f * (-DeltaUV2.x * Edge1.z + DeltaUV1.x * Edge2.z)
);
