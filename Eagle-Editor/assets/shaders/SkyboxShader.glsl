#type vertex
#version 450

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 0) uniform Matrices
{
	mat4 u_View;
	mat4 u_Projection;
};

out vec3 v_TexCoord;

void main()
{
	v_TexCoord = a_Position;

	mat3 myView3 = mat3(u_View);
	mat4 myView4 = mat4(myView3);

	gl_Position = u_Projection * myView4 * vec4(a_Position, 1.0);
	gl_Position = gl_Position.xyww;
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;

in vec3 v_TexCoord;

layout(std140, binding = 3) uniform GlobalSettings
{
	vec3 u_ViewPos;
	float u_Gamma;
	float u_Exposure;
};

uniform samplerCube u_Skybox;

void main()
{
	color = texture(u_Skybox, v_TexCoord);
	color.rgb = pow(color.rgb, vec3(1.f/ u_Gamma));
	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
}