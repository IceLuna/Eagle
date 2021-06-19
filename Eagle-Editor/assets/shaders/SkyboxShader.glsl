#type vertex
#version 450

layout(location = 0) in vec3 a_Position;

uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_TexCoord;

void main()
{
	v_TexCoord = a_Position;

	gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
	gl_Position = gl_Position.xyww;
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;

in vec3 v_TexCoord;

uniform samplerCube u_Skybox;

void main()
{
	color = texture(u_Skybox, v_TexCoord);
	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
}