#type vertex
#version 330 core

layout(location = 0) in vec3 a_Positions;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

void main()
{
	gl_Position = u_ViewProjection * u_Transform * vec4(a_Positions, 1.0);
}

#type fragment
#version 330 core

out vec4 color;

uniform vec4 u_Color;

void main()
{
	color = u_Color;
}
