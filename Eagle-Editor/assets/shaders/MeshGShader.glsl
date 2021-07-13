#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoord;
layout(location = 4) in int a_Index;

struct BatchData
{
	vec4 TintColor;
	int EntityID;
	float TilingFactor;
	float Shininess;
};

layout(std140, binding = 0) uniform Matrices
{
	mat4 u_View;
	mat4 u_Projection;
};

#define BATCH_SIZE 8
layout(std140, binding = 2) uniform Batch
{
	mat4 u_Models[BATCH_SIZE]; //960
	BatchData u_BatchData[BATCH_SIZE]; //240
}; // Total size = 1200.

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;
flat out int v_Index;
out mat3 v_TBN;

void main()
{
	const vec4 modelPosition = u_Models[a_Index] * vec4(a_Position, 1.0);
	gl_Position = u_Projection * u_View * modelPosition;
	
	v_Position = modelPosition.xyz;

	v_Normal   = mat3(transpose(inverse(u_Models[a_Index]))) * a_Normal;
	v_TexCoord = a_TexCoord;
	v_Index = a_Index;
	vec3 tangent = normalize(vec3(u_Models[a_Index] * vec4(a_Tangent, 0.0)));
	vec3 normal = normalize(vec3(u_Models[a_Index] * vec4(a_Normal, 0.0)));
	tangent = normalize(tangent - (normal * dot(tangent, normal)));
	vec3 bitangent = normalize(cross(normal, tangent));
	v_TBN = transpose(mat3(tangent, bitangent, normal));
}

#type fragment
#version 450

layout (location = 0) out vec3 o_Pos;
layout (location = 1) out vec3 o_Normal;
layout (location = 2) out vec4 o_AlbedoSpec;
layout (location = 3) out int o_EntityID;

in vec3  v_Position;
in vec3  v_Normal;
in vec2  v_TexCoord;
flat in int v_Index;
in mat3 v_TBN;

struct BatchData
{
	vec4 TintColor;
	int EntityID;
	float TilingFactor;
	float Shininess;
};

#define BATCH_SIZE 8
layout(std140, binding = 2) uniform Batch
{
	mat4 u_Models[BATCH_SIZE]; //960
	BatchData u_BatchData[BATCH_SIZE];
}; // Total size = 1680.

uniform int u_DiffuseTextureSlotIndexes[BATCH_SIZE];
uniform int u_SpecularTextureSlotIndexes[BATCH_SIZE];
uniform int u_NormalTextureSlotIndexes[BATCH_SIZE];
uniform sampler2D u_Textures[32];

void main()
{
	const vec2 tiledTexCoords = v_TexCoord * u_BatchData[v_Index].TilingFactor;

	vec3 normal = normalize(v_Normal);
	if (u_NormalTextureSlotIndexes[v_Index] != -1)
	{
		normal = texture(u_Textures[u_NormalTextureSlotIndexes[v_Index]], tiledTexCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(v_TBN * normal);
	}

	o_Pos = v_Position;
	o_Normal = normal;

	vec4 diffuseColor = vec4(1.0);
	if (u_DiffuseTextureSlotIndexes[v_Index] != -1)
		diffuseColor = texture(u_Textures[u_DiffuseTextureSlotIndexes[v_Index]], tiledTexCoords);
	diffuseColor *= u_BatchData[v_Index].TintColor;

	vec3 specularColor = vec3(0.0);
	if (u_SpecularTextureSlotIndexes[v_Index] != -1)
		specularColor = texture(u_Textures[u_SpecularTextureSlotIndexes[v_Index]], tiledTexCoords).rgb;

	o_AlbedoSpec.rgb = diffuseColor.rgb;
	o_AlbedoSpec.a = specularColor.r;
	o_EntityID = u_BatchData[v_Index].EntityID;
}
