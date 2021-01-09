#type vertex
#version 330 core

layout(location = 0) in vec3 a_Positions;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

void main()
{
	gl_Position = u_ViewProjection * u_Transform * vec4(a_Positions, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;
in vec4 gl_FragCoord;

uniform vec2 u_Resolution;
uniform float u_Time;
uniform float u_Cover;
uniform float u_ShowCoef;

void main()
{
	vec2 uv = gl_FragCoord.xy / u_Resolution.xy;

	vec3 col = 0.5f + 0.5f * cos(u_Time + uv.xyx + vec3(0, 2, 4));
	vec3 temp = col * sin(u_Cover - uv.y) * 0.5f;

	color = vec4(temp, 1.f) * 2.f * u_ShowCoef;
	
}