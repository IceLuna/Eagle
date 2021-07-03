#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 3) in int a_Index;

#define MAXSPOTLIGHTS 4
#define MAXPOINTLIGHTS 4

struct PointLight
{
	vec3 Position; //16 0

	vec3 Ambient; //16 16
	vec3 Diffuse; //16 32
	vec3 Specular;//12 48
	float Distance;//4 60
}; //Total Size = 64

struct DirectionalLight
{
	mat4 View; //64 0
	vec3 Direction; //16 64

	vec3 Ambient; //16 80
	vec3 Diffuse; //16 96
	vec3 Specular;//16 112
}; //Total Size = 128

struct SpotLight
{
	vec3 Position; //16 0
	vec3 Direction;//16 16

	vec3 Ambient;//16 32
	vec3 Diffuse;//16 48
	vec3 Specular;//12 64
	float InnerCutOffAngle;//4 76
	float OuterCutOffAngle;//4 80
}; //Total Size in Uniform buffer = 96

struct BatchData
{
	int EntityID;
	float TilingFactor;
	float Shininess;
};

layout(std140, binding = 0) uniform Matrices
{
	mat4 u_View;
	mat4 u_Projection;
};

layout(std140, binding = 1) uniform Lights
{
	DirectionalLight u_DirectionalLight; //128
	SpotLight u_SpotLights[MAXSPOTLIGHTS]; //96 * MAXSPOTLIGHTS
	PointLight u_PointLights[MAXPOINTLIGHTS]; //64 * MAXPOINTLIGHTS
	int u_PointLightsSize; //4
	int u_SpotLightsSize; //4
}; //Total Size = 720 + 64

#define BATCH_SIZE 15
layout(std140, binding = 2) uniform Batch
{
	mat4 u_Models[BATCH_SIZE]; //960
	BatchData u_BatchData[BATCH_SIZE]; //240
}; // Total size = 1200.

void main()
{
	gl_Position = u_Projection * u_DirectionalLight.View * u_Models[a_Index] * vec4(a_Position, 1.0);
}

#type fragment
#version 450

void main()
{
	// gl_FragDepth = gl_FragCoord.z;
}