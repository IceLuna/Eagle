#type vertex
#version 450

layout(location = 0) in vec3    a_Position;
layout(location = 1) in vec3    a_Normal;
layout(location = 2) in vec3    a_ModelNormal;
layout(location = 3) in vec3    a_Tangent;
layout(location = 4) in vec2    a_TexCoord;
layout(location = 5) in int     a_EntityID;
layout(location = 6) in int     a_DiffuseTextureIndex;
layout(location = 7) in int     a_SpecularTextureIndex;
layout(location = 8) in int     a_NormalTextureIndex;

//Material
layout(location = 9) in vec4   a_TintColor;
layout(location = 10) in float   a_TilingFactor;
layout(location = 11) in float  a_MaterialShininess;

layout(std140, binding = 0) uniform Matrices
{
	mat4 u_View;
	mat4 u_Projection;
};

out vec3  v_Position;
out vec3  v_Normal;
out vec2  v_TexCoord;
flat out int  v_DiffuseTextureIndex;
flat out int  v_SpecularTextureIndex;
flat out int  v_NormalTextureIndex;
flat out int  v_EntityID;
out mat3 v_TBN;

out v_MATERIAL
{
	vec4 TintColor;
	float TilingFactor;
	float Shininess;
}v_Material;

void main()
{
	v_Position = a_Position;

	v_Normal = a_Normal;
	v_TexCoord = a_TexCoord;
	v_EntityID = a_EntityID;
	v_DiffuseTextureIndex = a_DiffuseTextureIndex;
	v_SpecularTextureIndex = a_SpecularTextureIndex;
	v_NormalTextureIndex = a_NormalTextureIndex;

	vec3 T = normalize(a_Tangent - (a_ModelNormal * dot(a_Tangent, a_ModelNormal)));
	vec3 bitangent = normalize(cross(a_ModelNormal, T));
	v_TBN = transpose(mat3(T, bitangent, a_ModelNormal));

	//Material
	v_Material.TintColor = a_TintColor;
	v_Material.TilingFactor = a_TilingFactor;
	v_Material.Shininess = a_MaterialShininess;

	gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}

#type fragment
#version 450

layout(location = 0) out vec3 o_Pos;
layout(location = 1) out vec3 o_Normal;
layout(location = 2) out vec4 o_AlbedoSpec;
layout(location = 3) out int o_EntityID;

in vec3  v_Position;
in vec3  v_Normal;
in vec2  v_TexCoord;
flat in int	 v_DiffuseTextureIndex;
flat in int	 v_SpecularTextureIndex;
flat in int	 v_NormalTextureIndex;
flat in int	 v_EntityID;
in mat3 v_TBN;

in v_MATERIAL
{
	vec4 TintColor;
	float TilingFactor;
	float Shininess;
}v_Material;

uniform sampler2D u_Textures[32];

void main()
{
	vec2 tiledTexCoords = v_TexCoord * v_Material.TilingFactor;

	vec4 diffuseColor = vec4(1.0);
	if (v_DiffuseTextureIndex != -1)
		diffuseColor = texture(u_Textures[v_DiffuseTextureIndex], tiledTexCoords);
	diffuseColor *= v_Material.TintColor;

	vec3 specularColor = vec3(0.0);
	if (v_SpecularTextureIndex != -1)
		specularColor = texture(u_Textures[v_SpecularTextureIndex], tiledTexCoords).rgb;

	vec3 normal = normalize(v_Normal);
	if (v_NormalTextureIndex != -1)
	{
		normal = texture(u_Textures[v_NormalTextureIndex], tiledTexCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(v_TBN * normal);
	}

	o_Pos = v_Position;
	o_Normal = normal;

	o_AlbedoSpec.rgb = diffuseColor.rgb;
	o_AlbedoSpec.a = specularColor.r;
	o_EntityID = v_EntityID;
}
