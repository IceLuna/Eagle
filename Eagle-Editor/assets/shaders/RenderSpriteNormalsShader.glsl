#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

layout(std140, binding = 0) uniform Matrices
{
	mat4 u_View;
	mat4 u_Projection;
};

out VS_OUT
{
	vec3 v_Normals;
} vs_out;

void main()
{
	gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);

	//mat3 normalMatrix = mat3(transpose(inverse(a_Model)));
	vs_out.v_Normals = a_Normal; //normalize(vec3(u_Projection * vec4(normalMatrix * a_Normal, 0.0)));
}

#type geometry
#version 450

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

in VS_OUT{
	vec3 v_Normals;
} gs_in[];

const float MAGNITUDE = 0.2;

void GenerateLine(int index)
{
	gl_Position = gl_in[index].gl_Position;
	EmitVertex();
	gl_Position = gl_in[index].gl_Position + vec4(gs_in[index].v_Normals, 0.0) * MAGNITUDE;
	EmitVertex();
	EndPrimitive();
}

void main()
{
	GenerateLine(0); // ������ ������� ��� ������ �������
	GenerateLine(1); // ... ��� ������
	GenerateLine(2); // ... ��� �������
}

#type fragment
#version 450

out vec4 FragColor;

void main()
{
	FragColor = vec4(1.0, 1.0, 0.0, 1.0);
}
