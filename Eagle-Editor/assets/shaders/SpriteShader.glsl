#type vertex
#version 450

layout(location = 0) in vec3    a_Position;
layout(location = 1) in vec3    a_Normal;
layout(location = 2) in vec3    a_ModelNormal;
layout(location = 3) in vec3    a_Tangent;
layout(location = 4) in vec2    a_TexCoord;
layout(location = 5) in int     a_EntityID;
layout(location = 6) in int     a_DiffuseTextureIndex;
layout(location = 7) in int     a_SpecularTextureIndex;
layout(location = 8) in int     a_NormalTextureIndex;

//Material
layout(location = 9) in vec4   a_TintColor;
layout(location = 10) in float   a_TilingFactor;
layout(location = 11) in float  a_MaterialShininess;

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

out vec4 v_FragPosLightSpace;
out vec3  v_Position;
out vec3  v_Normal;
out vec2  v_TexCoord;
flat out int  v_DiffuseTextureIndex;
flat out int  v_SpecularTextureIndex;
flat out int  v_NormalTextureIndex;
flat out int  v_EntityID;
out mat3 v_TBN;

out v_MATERIAL
{
	vec4 TintColor;
	float TilingFactor;
	float Shininess;
}v_Material;

void main()
{
	v_Position = a_Position;
	v_FragPosLightSpace = u_DirectionalLight.ViewProj * vec4(a_Position, 1.0);

	v_Normal = a_Normal;
	v_TexCoord = a_TexCoord;
	v_EntityID = a_EntityID;
	v_DiffuseTextureIndex = a_DiffuseTextureIndex;
	v_SpecularTextureIndex = a_SpecularTextureIndex;
	v_NormalTextureIndex = a_NormalTextureIndex;

	vec3 T = normalize(a_Tangent - (a_ModelNormal * dot(a_Tangent, a_ModelNormal)));
	vec3 bitangent = normalize(cross(a_ModelNormal, T));
	v_TBN = transpose(mat3(T, bitangent, a_ModelNormal));

	//Material
	v_Material.TintColor = a_TintColor;
	v_Material.TilingFactor = a_TilingFactor;
	v_Material.Shininess = a_MaterialShininess;

	gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;
layout (location = 2) out int entityID;

in vec4 v_FragPosLightSpace;
in vec3  v_Position;
in vec3  v_Normal;
in vec2  v_TexCoord;
flat in int	 v_DiffuseTextureIndex;
flat in int	 v_SpecularTextureIndex;
flat in int	 v_NormalTextureIndex;
flat in int	 v_EntityID;
in mat3 v_TBN;

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
	vec3 Position; //16 0
	vec3 Direction;//16 16

	vec3 Ambient;//16 32
	vec3 Diffuse;//16 48
	vec3 Specular;//12 64
	float InnerCutOffAngle;//4 76
	float OuterCutOffAngle;//4 80
}; //Total Size in Uniform buffer = 96

in v_MATERIAL
{
	vec4 TintColor;
	float TilingFactor;
	float Shininess;
}v_Material;

#define MAXPOINTLIGHTS 4
#define MAXSPOTLIGHTS 4

layout(std140, binding = 1) uniform Lights
{
	DirectionalLight u_DirectionalLight; //128
	SpotLight u_SpotLights[MAXSPOTLIGHTS]; //96 * MAXSPOTLIGHTS
	PointLight u_PointLights[MAXPOINTLIGHTS]; //64 * MAXPOINTLIGHTS
	int u_PointLightsSize; //4
	int u_SpotLightsSize; //4
}; //Total Size = 720 + 64

layout(std140, binding = 3) uniform GlobalSettings
{
	float u_Gamma;
	float u_Exposure;
};

uniform vec3 u_ViewPos;
uniform sampler2D u_Textures[32];
uniform sampler2D u_ShadowMap;
uniform samplerCube u_Skybox;
uniform samplerCube u_PointShadowCubemaps[MAXPOINTLIGHTS];
uniform int u_SkyboxEnabled;

vec3 CalculatePointLight(PointLight pointLight, samplerCube shadowMap);
vec3 CalculateDirectionalLight(DirectionalLight directionalLight);
vec3 CalculateSpotLight(SpotLight spotLight);
float CalculateDirectionalShadow(vec4 fragPosLightSpace); //If shadowed, returns 1.0 else returns 0.0
float CalculatePointShadow(PointLight pointLight, vec3 fragPos, samplerCube shadowMap);
vec3 CalculateSkyboxLight();

vec2 g_TiledTexCoords;
const float g_FarPlane = 10000.f;

void main()
{
	vec3 pointLightsResult = vec3(0.0);
	vec3 spotLightsResult = vec3(0.0);
	g_TiledTexCoords = v_TexCoord * v_Material.TilingFactor;

	for (int i = 0; i < u_PointLightsSize; ++i)
	{
		pointLightsResult += CalculatePointLight(u_PointLights[i], u_PointShadowCubemaps[i]);
	}

	for (int i = 0; i < u_SpotLightsSize; ++i)
	{
		spotLightsResult += CalculateSpotLight(u_SpotLights[i]);
	}

	vec3 directionalLightResult = CalculateDirectionalLight(u_DirectionalLight);
	vec3 skyboxLight = vec3(0.0);

	if (u_SkyboxEnabled == 1) 
		skyboxLight = CalculateSkyboxLight();

	double diffuseAlpha = 1.0;
	//if (v_DiffuseTextureIndex != -1)
	//	diffuseAlpha = texture(u_Textures[v_DiffuseTextureIndex], g_TiledTexCoords).a;
	//diffuseAlpha *= v_Material.TintColor.a;
	vec3 result = pointLightsResult + directionalLightResult + spotLightsResult + skyboxLight;
	result.rgb = vec3(1.0) - exp(-result.rgb * u_Exposure);
	color = vec4(pow(result, vec3(1.f/ u_Gamma)), diffuseAlpha);

	//Other stuff
	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
	entityID = v_EntityID;
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
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	if (projCoords.z > 1.0)
		return 0.0f;

	float shadow = 0.0f;

	float bias = 0.00005f;
	float currentDepth = projCoords.z;

	vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			float val = (currentDepth - bias) > pcfDepth ? 1.0f : 0.0f;
			shadow += val;
		}
	}
	shadow /= 9.0f;
	return shadow;
}

vec3 CalculateSkyboxLight()
{
	vec3 normal = normalize(v_Normal);
	if (v_NormalTextureIndex != -1)
	{
		normal = texture(u_Textures[v_NormalTextureIndex], g_TiledTexCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(v_TBN * normal);
	}
	vec3 viewDir = normalize(v_Position - u_ViewPos);
	vec3 R = reflect(viewDir, normal);
	vec3 result = texture(u_Skybox, R).rgb;

	vec3 specularColor = vec3(0.0);
	if (v_SpecularTextureIndex != -1)
		specularColor = texture(u_Textures[v_SpecularTextureIndex], g_TiledTexCoords).rgb;

	result = result * specularColor;

	return result;
}

vec3 CalculateSpotLight(SpotLight spotLight)
{
	vec3 n_LightDir = normalize(spotLight.Position - v_Position);
	
	float theta = dot(n_LightDir, normalize(-spotLight.Direction));

	float innerCutOffCos = cos(radians(spotLight.InnerCutOffAngle));
	float outerCutOffCos = cos(radians(spotLight.OuterCutOffAngle));

	//Cutoff
	float epsilon = innerCutOffCos - outerCutOffCos;
	float intensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);

	//Diffuse
	vec3 n_Normal = normalize(v_Normal);
	if (v_NormalTextureIndex != -1)
	{
		n_Normal = texture(u_Textures[v_NormalTextureIndex], g_TiledTexCoords).rgb;
		n_Normal = normalize(n_Normal * 2.0 - 1.0);
		n_Normal = normalize(v_TBN * n_Normal);
	}
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec4 diffuseColor = vec4(1.0);
	if (v_DiffuseTextureIndex != -1)
		diffuseColor = texture(u_Textures[v_DiffuseTextureIndex], g_TiledTexCoords);
	diffuseColor *= v_Material.TintColor;
	vec3 diffuse = (diff * diffuseColor.rgb) * spotLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), v_Material.Shininess);
	vec4 specularColor = vec4(0.0);
	if (v_SpecularTextureIndex != -1)
		specularColor = texture(u_Textures[v_SpecularTextureIndex], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * spotLight.Specular * spotLight.Diffuse;

	vec3 ambient = diffuseColor.rgb * spotLight.Ambient * spotLight.Diffuse;

	//Result
	vec3 result = intensity * (diffuse + specular + ambient);
	return result;
}

vec3 CalculateDirectionalLight(DirectionalLight directionalLight)
{
	vec4 diffuseColor = vec4(1.0);
	if (v_DiffuseTextureIndex != -1)
		diffuseColor = texture(u_Textures[v_DiffuseTextureIndex], g_TiledTexCoords);
	diffuseColor *= v_Material.TintColor;
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	float shadow = CalculateDirectionalShadow(v_FragPosLightSpace);

	//Diffuse
	vec3 normal = normalize(v_Normal);
	if (v_NormalTextureIndex != -1)
	{
		normal = texture(u_Textures[v_NormalTextureIndex], g_TiledTexCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(v_TBN * normal);
	}
	vec3 n_Normal = normal;
	vec3 n_LightDir = normalize(-directionalLight.Direction);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	diffuse = (diff * diffuseColor.rgb) * directionalLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), v_Material.Shininess);
	vec4 specularColor = vec4(0.0);
	if (v_SpecularTextureIndex != -1)
		specularColor = texture(u_Textures[v_SpecularTextureIndex], g_TiledTexCoords);
	specular = specularColor.rgb * specCoef * directionalLight.Specular * directionalLight.Diffuse;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * directionalLight.Ambient * directionalLight.Diffuse;

	//Result
	vec3 result = ((1.0f - shadow) * (diffuse + specular) + ambient);
	return result;
}

vec3 CalculatePointLight(PointLight pointLight, samplerCube shadowMap)
{
	//const float KLin = 0.09, KSq = 0.032;
	const float KLin = 0.007, KSq = 0.0002;
	float distance = length(pointLight.Position - v_Position);
	distance *= distance / pointLight.Distance;
	float attenuation = 1.0 / (1.0 + KLin * distance +  KSq * (distance * distance));

	float shadow = CalculatePointShadow(pointLight, v_Position, shadowMap);

	//Diffuse
	vec3 n_Normal = normalize(v_Normal);
	if (v_NormalTextureIndex != -1)
	{
		n_Normal = texture(u_Textures[v_NormalTextureIndex], g_TiledTexCoords).rgb;
		n_Normal = normalize(n_Normal * 2.0 - 1.0);
		n_Normal = normalize(v_TBN * n_Normal);
	}
	vec3 n_LightDir = normalize(pointLight.Position - v_Position);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec4 diffuseColor = vec4(1.0);
	if (v_DiffuseTextureIndex != -1)
		diffuseColor = texture(u_Textures[v_DiffuseTextureIndex], g_TiledTexCoords);
	diffuseColor *= v_Material.TintColor;
	vec3 diffuse = (diff * diffuseColor.rgb) * pointLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), v_Material.Shininess);
	vec4 specularColor = vec4(0.0);
	if (v_SpecularTextureIndex != -1)
		specularColor = texture(u_Textures[v_SpecularTextureIndex], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * pointLight.Specular * pointLight.Diffuse;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * pointLight.Ambient * pointLight.Diffuse;

	//Result
	vec3 result = attenuation * ((1.0f - shadow) * (diffuse + specular) + ambient);
	return result;
}
