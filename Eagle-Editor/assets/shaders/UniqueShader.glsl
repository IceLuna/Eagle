#type vertex
#version 450

layout(location = 0) in vec3    a_Position;
layout(location = 1) in vec3    a_Normal;
layout(location = 2) in vec2    a_TexCoord;
layout(location = 3) in int     a_EntityID;
layout(location = 4) in int     a_DiffuseTextureIndex;
layout(location = 5) in int     a_SpecularTextureIndex;
layout(location = 6) in float   a_TilingFactor;

//Material
layout(location = 7) in float  a_MaterialShininess;

uniform mat4 u_ViewProjection;

out vec3  v_Position;
out vec3  v_Normal;
out vec2  v_TexCoord;
flat out int  v_DiffuseTextureIndex;
flat out int  v_SpecularTextureIndex;
flat out int  v_EntityID;
out float v_TilingFactor;

out v_MATERIAL
{
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
	v_TilingFactor = a_TilingFactor;

	//Material
	v_Material.Shininess = a_MaterialShininess;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;
layout (location = 2) out int entityID;

in vec3  v_Position;
in vec3  v_Normal;
in vec2  v_TexCoord;
flat in int	 v_DiffuseTextureIndex;
flat in int	 v_SpecularTextureIndex;
flat in int	 v_EntityID;
in float v_TilingFactor;

struct Light
{
	vec3 Position;

	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	float Distance;
};

in v_MATERIAL
{
	float Shininess;
}v_Material;

uniform vec3 u_ViewPos;
uniform sampler2D u_DiffuseTextures[16];
uniform sampler2D u_SpecularTextures[16];
uniform Light light;

void main()
{
	//const float KLin = 0.09, KSq = 0.032;
	const float KLin = 0.007, KSq = 0.0002;
	float distance = length(light.Position - v_Position);
	distance *= distance / light.Distance;
	float attenuation = 1.0 / (1.0 + KLin * distance +  KSq * (distance * distance));

	//Diffuse
	vec3 n_Normal = normalize(v_Normal);
	vec3 n_LightDir = normalize(light.Position - v_Position);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec4 diffuseColor = texture(u_DiffuseTextures[v_DiffuseTextureIndex], v_TexCoord * v_TilingFactor);
	vec3 diffuse = (diff * diffuseColor.rgb) * light.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 reflectDir = reflect(-n_LightDir, n_Normal);
	float specCoef = pow(max(dot(viewDir, reflectDir), 0.0), v_Material.Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_SpecularTextureIndex], v_TexCoord);
	vec3 specular = specularColor.rgb * specCoef * light.Specular;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * light.Ambient;

	color = vec4(vec3(attenuation*(diffuse + ambient + specular)), 1.0);

	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
	entityID = v_EntityID;
}