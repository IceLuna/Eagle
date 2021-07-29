#type vertex
#version 450

layout(location = 0) in vec3 a_Position;

struct PointLight
{
	mat4 ViewProj[6];
	vec3 Position; //16 0

	vec3 Ambient; //16 16
	vec3 Diffuse; //16 32
	vec3 Specular;//12 48
	float Distance;//4 60
}; //Total Size = 64

struct DirectionalLight
{
	mat4 ViewProj; //64 0
	vec3 Direction; //16 64

	vec3 Ambient; //16 80
	vec3 Diffuse; //16 96
	vec3 Specular;//16 112
}; //Total Size = 128

struct SpotLight
{
	mat4 ViewProj; //64
	vec3 Position; //16 0
	vec3 Direction;//16 16

	vec3 Ambient;//16 32
	vec3 Diffuse;//16 48
	vec3 Specular;//12 64
	float InnerCutOffAngle;//4 76
	float OuterCutOffAngle;//4 80
}; //Total Size in Uniform buffer = 96

#define MAXSPOTLIGHTS 4
#define MAXPOINTLIGHTS 4

layout(std140, binding = 1) uniform Lights
{
	DirectionalLight u_DirectionalLight; //128
	SpotLight u_SpotLights[MAXSPOTLIGHTS]; //96 * MAXSPOTLIGHTS
	PointLight u_PointLights[MAXPOINTLIGHTS]; //64 * MAXPOINTLIGHTS
	int u_PointLightsSize; //4
	int u_SpotLightsSize; //4
}; //Total Size = 720 + 64

uniform int u_SpotLightIndex;
out vec4 v_FragPos;

void main()
{
	v_FragPos = vec4(a_Position, 1.0);
    gl_Position = u_SpotLights[u_SpotLightIndex].ViewProj * v_FragPos;
}

#type fragment
#version 450

struct PointLight
{
	mat4 ViewProj[6];
	vec3 Position; //16 0

	vec3 Ambient; //16 16
	vec3 Diffuse; //16 32
	vec3 Specular;//12 48
	float Distance;//4 60
}; //Total Size = 64

struct DirectionalLight
{
	mat4 ViewProj; //64 0
	vec3 Direction; //16 64

	vec3 Ambient; //16 80
	vec3 Diffuse; //16 96
	vec3 Specular;//16 112
}; //Total Size = 128

struct SpotLight
{
	mat4 ViewProj; //64
	vec3 Position; //16 0
	vec3 Direction;//16 16

	vec3 Ambient;//16 32
	vec3 Diffuse;//16 48
	vec3 Specular;//12 64
	float InnerCutOffAngle;//4 76
	float OuterCutOffAngle;//4 80
}; //Total Size in Uniform buffer = 96

#define MAXSPOTLIGHTS 4
#define MAXPOINTLIGHTS 4

layout(std140, binding = 1) uniform Lights
{
	DirectionalLight u_DirectionalLight; //128
	SpotLight u_SpotLights[MAXSPOTLIGHTS]; //96 * MAXSPOTLIGHTS
	PointLight u_PointLights[MAXPOINTLIGHTS]; //64 * MAXPOINTLIGHTS
	int u_PointLightsSize; //4
	int u_SpotLightsSize; //4
}; //Total Size = 720 + 64

uniform int u_SpotLightIndex;
in vec4 v_FragPos;
const float g_FarPlane = 10000.f;

bool DoesSpotLightAffect(SpotLight spotLight)
{
	vec3 n_LightDir = normalize(spotLight.Position - v_FragPos.xyz);

	float theta = dot(n_LightDir, normalize(-spotLight.Direction));

	float innerCutOffCos = cos(radians(spotLight.InnerCutOffAngle));
	float outerCutOffCos = cos(radians(spotLight.OuterCutOffAngle));

	//Cutoff
	float epsilon = innerCutOffCos - outerCutOffCos;
	float intensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);

	return (intensity != 0.000000f);
}

void main()
{
	bool bAffects = DoesSpotLightAffect(u_SpotLights[u_SpotLightIndex]);
	gl_FragDepth = bAffects ? gl_FragCoord.z : 1.0;

	//
	//float distance = length(v_FragPos.xyz - u_SpotLights[u_SpotLightIndex].Position);
	//distance /= g_FarPlane;
	////gl_FragDepth = bAffects ? distance : g_FarPlane;
	//gl_FragDepth = distance;
}