#type vertex
#version 450

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec4  a_Color;
layout(location = 3) in vec2  a_TexCoord;
layout(location = 4) in int   a_EntityID;
layout(location = 5) in int   a_TextureIndex;
layout(location = 6) in float a_TilingFactor;

uniform mat4 u_ViewProjection;

out vec3  v_Position;
out vec3  v_Normal;
out vec4  v_Color;
out vec2  v_TexCoord;
flat out int  v_TextureIndex;
flat out int  v_EntityID;
out float v_TilingFactor;

void main()
{
	v_Position = a_Position;
	v_Normal = a_Normal;
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_TextureIndex = a_TextureIndex;
	v_EntityID = a_EntityID;
	v_TilingFactor = a_TilingFactor;
	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;
layout (location = 2) out int entityID;

in vec3  v_Position;
in vec3  v_Normal;
in vec4  v_Color;
in vec2  v_TexCoord;
flat in int	 v_TextureIndex;
flat in int	 v_EntityID;
in float v_TilingFactor;

uniform vec3 u_ViewPos;
uniform sampler2D u_Textures[32];
uniform vec4 u_LightColor;
uniform vec3 u_LightTranslation;
uniform float u_Ambient;
uniform float u_Specular;
uniform int u_SpecularPower;

void main()
{
	color = texture(u_Textures[v_TextureIndex], v_TexCoord * v_TilingFactor) * v_Color;

	//Ambient
	vec3 ambient = u_Ambient * u_LightColor.rgb;
	
	//Diffuse
	vec3 n_Normal = normalize(v_Normal);
	vec3 n_LightDir = normalize(u_LightTranslation - v_Position);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec3 diffuse = diff * u_LightColor.rgb;

	//Specular
	vec3 viewDir = normalize(-u_ViewPos - v_Position);
	vec3 reflectDir = reflect(n_LightDir, n_Normal);
	float specCoef = pow(max(dot(viewDir, reflectDir), 0.0), u_SpecularPower);
	vec3 specular = u_Specular * specCoef * u_LightColor.rgb;

	color *= vec4(vec3(diffuse + ambient + specular), 1.0);

	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
	entityID = v_EntityID;
}