#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform Matrices
{
	mat4 u_View;
	mat4 u_Projection;
};

out vec4 v_Color;

void main()
{
	gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
	v_Color = a_Color;
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out int entityID;

in vec4 v_Color;

void main()
{
	color = v_Color;
	entityID = -1;
}
