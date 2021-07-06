#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 3) in int a_Index;

layout(std140, binding = 0) uniform Matrices
{
	mat4 u_View;
	mat4 u_Projection;
};

struct BatchData
{
	int EntityID;
	float TilingFactor;
	float Shininess;
};

#define BATCH_SIZE 15
layout(std140, binding = 2) uniform Batch
{
	mat4 u_Models[BATCH_SIZE]; //960
	BatchData u_BatchData[BATCH_SIZE]; //240
}; // Total size = 1200.

out VS_OUT
{
	vec3 v_Normals;
} vs_out;

void main()
{
	gl_Position = u_Projection * u_View * u_Models[a_Index] * vec4(a_Position, 1.0);

	mat3 normalMatrix = mat3(transpose(inverse(u_Models[a_Index])));
	vs_out.v_Normals = normalize(vec3(u_Projection * vec4(normalMatrix * a_Normal, 0.0)));
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
	GenerateLine(0); // вектор нормали для первой вершины
	GenerateLine(1); // ... для второй
	GenerateLine(2); // ... для третьей
}

#type fragment
#version 450

out vec4 FragColor;

void main()
{
	FragColor = vec4(1.0, 1.0, 0.0, 1.0);
}
