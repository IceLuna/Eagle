#type vertex
#version 450

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec2  a_TexCoord;
layout(location = 3) in int   a_EntityID;
layout(location = 4) in int   a_TextureIndex;
layout(location = 5) in float a_TilingFactor;

//Material
layout(location = 6) in vec4  a_MaterialDiffuse;
layout(location = 7) in vec3  a_MaterialAmbient;
layout(location = 8) in vec3  a_MaterialSpecular;
layout(location = 9) in float a_MaterialShininess;

uniform mat4 u_ViewProjection;

out vec3  v_Position;
out vec3  v_Normal;
out vec2  v_TexCoord;
flat out int  v_TextureIndex;
flat out int  v_EntityID;
out float v_TilingFactor;

out v_MATERIAL
{
	vec4 Diffuse;
	vec3 Ambient;
	vec3 Specular;
	float Shininess;
}v_Material;

void main()
{
	v_Position = a_Position;
	v_Normal = a_Normal;
	v_TexCoord = a_TexCoord;
	v_TextureIndex = a_TextureIndex;
	v_EntityID = a_EntityID;
	v_TilingFactor = a_TilingFactor;

	//Material
	v_Material.Diffuse = a_MaterialDiffuse;
	v_Material.Ambient = a_MaterialAmbient;
	v_Material.Specular = a_MaterialSpecular;
	v_Material.Shininess = a_MaterialShininess;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450

struct Light
{
	vec3 Position;

	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
};

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;
layout (location = 2) out int entityID;

in vec3  v_Position;
in vec3  v_Normal;
in vec2  v_TexCoord;
flat in int	 v_TextureIndex;
flat in int	 v_EntityID;
in float v_TilingFactor;

in v_MATERIAL
{
	vec4 Diffuse;
	vec3 Ambient;
	vec3 Specular;
	float Shininess;
}v_Material;

uniform vec3 u_ViewPos;
uniform sampler2D u_Textures[32];
uniform Light light;

void main()
{
	color = texture(u_Textures[v_TextureIndex], v_TexCoord * v_TilingFactor) * v_Material.Diffuse;

	//Ambient
	vec3 ambient = v_Material.Ambient * light.Ambient;
	
	//Diffuse
	vec3 n_Normal = normalize(v_Normal);
	vec3 n_LightDir = normalize(light.Position - v_Position);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec3 diffuse = (diff * v_Material.Diffuse.rgb) * light.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 reflectDir = reflect(-n_LightDir, n_Normal);
	float specCoef = pow(max(dot(viewDir, reflectDir), 0.0), v_Material.Shininess);
	vec3 specular = v_Material.Specular * specCoef * light.Specular;

	color *= vec4(vec3(diffuse + ambient + specular), 1.0);

	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
	entityID = v_EntityID;
}