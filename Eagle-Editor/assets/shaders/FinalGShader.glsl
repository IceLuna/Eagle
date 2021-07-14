#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(std140, binding = 0) uniform Matrices
{
	mat4 u_View;
	mat4 u_Projection;
};

out vec2 v_TexCoords;

void main()
{
	gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);

	v_TexCoords = a_TexCoord;
}

#type fragment
#version 450

layout (location = 0) out vec4 o_Color;

in vec2 v_TexCoords;

struct PointLight
{
	mat4 ViewProj[6];
	vec3 Position; //16 0

	vec3 Ambient; //16 16
	vec3 Diffuse; //16 32
	vec3 Specular;//12 48
	float Distance;//4 60
}; //Total Size = 384+64

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
	vec3 Position; //16 0
	vec3 Direction;//16 16

	vec3 Ambient;//16 32
	vec3 Diffuse;//16 48
	vec3 Specular;//12 64
	float InnerCutOffAngle;//4 76
	float OuterCutOffAngle;//4 80
}; //Total Size in Uniform buffer = 96

#define MAXPOINTLIGHTS 4
#define MAXSPOTLIGHTS 4

layout(std140, binding = 1) uniform Lights
{
	DirectionalLight u_DirectionalLight; //128
	SpotLight u_SpotLights[MAXSPOTLIGHTS]; //96 * MAXSPOTLIGHTS
	PointLight u_PointLights[MAXPOINTLIGHTS]; //64 * MAXPOINTLIGHTS
	int u_PointLightsSize; //4
	int u_SpotLightsSize; //4
}; //Total Size = 2320

layout(std140, binding = 3) uniform GlobalSettings
{
	vec3 u_ViewPos;
	float u_Gamma;
	float u_Exposure;
};

uniform sampler2D u_PositionTexture;
uniform sampler2D u_NormalsTexture;
uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_MaterialTexture;
uniform sampler2D u_ShadowMap;

uniform samplerCube u_Skybox;
uniform samplerCube u_PointShadowCubemaps[MAXPOINTLIGHTS];
uniform int u_SkyboxEnabled;

struct RenderData
{
	vec4 Albedo;
	vec3 FragPosition;
	vec3 Normal;
	float Shininess;
};

vec3 CalculatePointLight(PointLight pointLight, samplerCube shadowMap, RenderData renderData);
vec3 CalculateDirectionalLight(DirectionalLight directionalLight, RenderData renderData);
vec3 CalculateSpotLight(SpotLight spotLight, RenderData renderData);
vec3 CalculateSkyboxLight(RenderData renderData);
float CalculateDirectionalShadow(vec4 fragPosLightSpace);
float CalculatePointShadow(PointLight pointLight, vec3 fragPos, samplerCube shadowMap);

const float g_FarPlane = 10000.f;

void main()
{
	RenderData renderData;
	vec3 pointLightsResult = vec3(0.0);
	vec3 spotLightsResult = vec3(0.0);

	renderData.FragPosition = texture(u_PositionTexture, v_TexCoords).rgb;
	renderData.Normal = texture(u_NormalsTexture, v_TexCoords).rgb;
	renderData.Albedo = texture(u_AlbedoTexture, v_TexCoords);
	renderData.Shininess = texture(u_MaterialTexture, v_TexCoords).r;

	for (int i = 0; i < u_PointLightsSize; ++i)
	{
		pointLightsResult += CalculatePointLight(u_PointLights[i], u_PointShadowCubemaps[i], renderData);
	}

	for (int i = 0; i < u_SpotLightsSize; ++i)
	{
		spotLightsResult += CalculateSpotLight(u_SpotLights[i], renderData);
	}

	vec3 directionalLightResult = CalculateDirectionalLight(u_DirectionalLight, renderData);
	vec3 skyboxLight = vec3(0.0);

	if (u_SkyboxEnabled == 1)
		skyboxLight = CalculateSkyboxLight(renderData);

	vec3 result = pointLightsResult + directionalLightResult + spotLightsResult + skyboxLight;
	result.rgb = vec3(1.0) - exp(-result.rgb * u_Exposure);
	o_Color = vec4(pow(result, vec3(1.f/ u_Gamma)), 1.0);
}

float CalculatePointShadow(PointLight pointLight, vec3 fragPos, samplerCube shadowMap)
{
	const vec3 sampleOffsetDirections[20] = vec3[]
	(
		vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
		vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
		vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
		vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
		vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
	);

	vec3 fragToLight = fragPos - pointLight.Position;
	float viewDistance = length(u_ViewPos - fragPos);
	float diskRadius = (1.0f + (viewDistance / g_FarPlane)) / g_FarPlane;
	float currentDepth = length(fragToLight);

	float shadow = 0.0f;
	float bias = 0.002f;
	int samples = 20;

	for (int i = 0; i < samples; ++i)
	{
		float closestDepth = texture(shadowMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
		closestDepth *= g_FarPlane;
		if (currentDepth - bias > closestDepth)
			shadow += 1.0;
	}
	shadow /= float(samples);

	return shadow;
}

float CalculateDirectionalShadow(vec4 fragPosLightSpace)
{
	float shadow = 0.0;

	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

	projCoords = projCoords * 0.5 + 0.5;

	if (projCoords.z > 1.0)
		return shadow;

	float bias = 0.0001f;
	float currentDepth = projCoords.z;

	vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 9.0;
	return shadow;
}

vec3 CalculateSkyboxLight(RenderData renderData)
{
	vec3 normal = renderData.Normal;

	vec3 viewDir = normalize(renderData.FragPosition - u_ViewPos);
	vec3 R = reflect(viewDir, normal);
	vec3 result = texture(u_Skybox, R).rgb;

	float specularColor = renderData.Albedo.a;

	result = result * specularColor;

	return result;
}

vec3 CalculateSpotLight(SpotLight spotLight, RenderData renderData)
{
	vec3 n_LightDir = normalize(spotLight.Position - renderData.FragPosition);
	
	float theta = dot(n_LightDir, normalize(-spotLight.Direction));

	float innerCutOffCos = cos(radians(spotLight.InnerCutOffAngle));
	float outerCutOffCos = cos(radians(spotLight.OuterCutOffAngle));

	//Cutoff
	float epsilon = innerCutOffCos - outerCutOffCos;
	float intensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);

	//Diffuse
	vec3 n_Normal = renderData.Normal;

	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec3 diffuseColor = renderData.Albedo.rgb;
	vec3 diffuse = (diff * diffuseColor) * spotLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - renderData.FragPosition);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), renderData.Shininess);
	float specularColor = renderData.Albedo.a;

	vec3 specular = specularColor * specCoef * spotLight.Specular * spotLight.Diffuse;

	vec3 ambient = diffuseColor.rgb * spotLight.Ambient * spotLight.Diffuse;

	//Result
	vec3 result = intensity * (diffuse + specular + ambient);
	return result;
}

vec3 CalculateDirectionalLight(DirectionalLight directionalLight, RenderData renderData)
{
	vec4 fragPosLightSpace = directionalLight.ViewProj * vec4(renderData.FragPosition, 1.0);
	float shadow = CalculateDirectionalShadow(fragPosLightSpace);
	
	//Diffuse
	vec3 n_Normal = renderData.Normal;

	vec3 n_LightDir = normalize(-directionalLight.Direction);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec3 diffuseColor = renderData.Albedo.rgb;

	vec3 diffuse = (diff * diffuseColor) * directionalLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - renderData.FragPosition);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), renderData.Shininess);
	float specularColor = renderData.Albedo.a;

	vec3 specular = specularColor * specCoef * directionalLight.Specular * directionalLight.Diffuse;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * directionalLight.Ambient * directionalLight.Diffuse;

	//Result
	vec3 result = ((1.0f - shadow) * (diffuse + specular) + ambient);
	return result;
}

vec3 CalculatePointLight(PointLight pointLight, samplerCube shadowMap, RenderData renderData)
{
	//const float KLin = 0.09, KSq = 0.032;
	const float KLin = 0.007, KSq = 0.0002;
	float distance = length(pointLight.Position - renderData.FragPosition);
	distance *= distance / pointLight.Distance;
	float attenuation = 1.0 / (1.0 + KLin * distance +  KSq * (distance * distance));
	float shadow = CalculatePointShadow(pointLight, renderData.FragPosition, shadowMap);
	//Diffuse
	vec3 n_Normal = renderData.Normal;

	vec3 n_LightDir = normalize(pointLight.Position - renderData.FragPosition);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec3 diffuseColor = renderData.Albedo.rgb;
	vec3 diffuse = (diff * diffuseColor) * pointLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - renderData.FragPosition);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), renderData.Shininess);
	float specularColor = renderData.Albedo.a;
	vec3 specular = specularColor * specCoef * pointLight.Specular * pointLight.Diffuse;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * pointLight.Ambient * pointLight.Diffuse;

	//Result
	vec3 result = attenuation * ((1.0f - shadow) * (diffuse + specular) + ambient);
	return result;
}