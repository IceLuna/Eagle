#type vertex
#version 450

layout(location = 0) in vec3    a_Position;
layout(location = 1) in vec2    a_TexCoord;
layout(location = 2) in int     a_EntityID;
layout(location = 3) in int     a_TextureIndex;
layout(location = 4) in float   a_TilingFactor;

uniform mat4 u_ViewProjection;

out vec3  v_Position;
out vec2  v_TexCoord;
flat out int  v_EntityID;
flat out int  v_TextureIndex;
out float v_TilingFactor;

void main()
{
	v_Position = a_Position;
	v_TexCoord = a_TexCoord;
	v_EntityID = a_EntityID;
	v_TextureIndex = a_TextureIndex;
	v_TilingFactor = a_TilingFactor;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;
layout (location = 2) out int entityID;

in vec3  v_Position;
in vec2  v_TexCoord;
flat in int	 v_EntityID;
flat in int	 v_TextureIndex;
in float v_TilingFactor;

uniform sampler2D u_Textures[32];

void main()
{
	vec4 color = texture(u_Textures[v_TextureIndex], v_TexCoord * v_TilingFactor);

	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
	entityID = v_EntityID;
}