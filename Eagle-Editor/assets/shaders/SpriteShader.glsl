#type vertex
#version 450

layout(location = 0) in vec3    a_Position;
layout(location = 1) in vec3    a_Normal;
layout(location = 2) in vec2    a_TexCoord;
layout(location = 3) in int     a_EntityID;
layout(location = 4) in int     a_DiffuseTextureIndex;
layout(location = 5) in int     a_SpecularTextureIndex;

//Material
layout(location = 6) in float   a_TilingFactor;
layout(location = 7) in float  a_MaterialShininess;

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
flat out int  v_EntityID;
out float v_TilingFactor;

out v_MATERIAL
{
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

	//Material
	v_TilingFactor = a_TilingFactor;
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
flat in int	 v_EntityID;
in float v_TilingFactor;

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

uniform vec3 u_ViewPos;
uniform sampler2D u_DiffuseTextures[16];
uniform sampler2D u_SpecularTextures[16];
uniform sampler2D u_ShadowMap;
uniform samplerCube u_Skybox;
uniform int u_SkyboxEnabled;
uniform float u_Gamma;

vec3 CalculatePointLight(PointLight pointLight);
vec3 CalculateDirectionalLight(DirectionalLight directionalLight);
vec3 CalculateSpotLight(SpotLight spotLight);
float CalculateShadow(vec4 fragPosLightSpace); //If shadowed, returns 1.0 else returns 0.0
vec3 CalculateSkyboxLight();

vec2 g_TiledTexCoords;

void main()
{
	vec3 pointLightsResult = vec3(0.0);
	vec3 spotLightsResult = vec3(0.0);
	g_TiledTexCoords = v_TexCoord * v_TilingFactor;

	for (int i = 0; i < u_PointLightsSize; ++i)
	{
		pointLightsResult += CalculatePointLight(u_PointLights[i]);
	}

	for (int i = 0; i < u_SpotLightsSize; ++i)
	{
		spotLightsResult += CalculateSpotLight(u_SpotLights[i]);
	}

	vec3 directionalLightResult = CalculateDirectionalLight(u_DirectionalLight);
	vec3 skyboxLight = vec3(0.0);

	if (u_SkyboxEnabled == 1) 
		skyboxLight = CalculateSkyboxLight();

	double diffuseAlpha = texture(u_DiffuseTextures[v_DiffuseTextureIndex], g_TiledTexCoords).a;
	vec3 result = pointLightsResult + directionalLightResult + spotLightsResult + skyboxLight;
	color = vec4(pow(result, vec3(1.f/ u_Gamma)), diffuseAlpha);

	//Other stuff
	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
	entityID = v_EntityID;
}

float CalculateShadow(vec4 fragPosLightSpace)
{
	float shadow = 0.0;

	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

	projCoords = projCoords * 0.5 + 0.5;

	if (projCoords.z > 1.0)
		return shadow;

	float bias = 0.00001f;
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

vec3 CalculateSkyboxLight()
{
	vec3 viewDir = normalize(v_Position - u_ViewPos);
	vec3 R = reflect(viewDir, normalize(v_Normal));
	vec3 result = texture(u_Skybox, R).rgb;

	vec3 specularColor = texture(u_SpecularTextures[v_SpecularTextureIndex], g_TiledTexCoords).rgb;

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
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec4 diffuseColor = texture(u_DiffuseTextures[v_DiffuseTextureIndex], g_TiledTexCoords);
	vec3 diffuse = (diff * diffuseColor.rgb) * spotLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), v_Material.Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_SpecularTextureIndex], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * spotLight.Specular * spotLight.Diffuse;

	vec3 ambient = diffuseColor.rgb * spotLight.Ambient * spotLight.Diffuse;

	//Result
	vec3 result = intensity * (diffuse + specular + ambient);
	return result;
}

vec3 CalculateDirectionalLight(DirectionalLight directionalLight)
{
	vec4 diffuseColor = texture(u_DiffuseTextures[v_DiffuseTextureIndex], g_TiledTexCoords);
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	float shadow = CalculateShadow(v_FragPosLightSpace);
	if (shadow < 1.0)
	{
		//Diffuse
		vec3 n_Normal = normalize(v_Normal);
		vec3 n_LightDir = normalize(-directionalLight.Direction);
		float diff = max(dot(n_Normal, n_LightDir), 0.0);
		diffuse = (diff * diffuseColor.rgb) * directionalLight.Diffuse;

		//Specular
		vec3 viewDir = normalize(u_ViewPos - v_Position);
		vec3 halfwayDir = normalize(n_LightDir + viewDir);
		float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), v_Material.Shininess);
		vec4 specularColor = texture(u_SpecularTextures[v_SpecularTextureIndex], g_TiledTexCoords);
		specular = specularColor.rgb * specCoef * directionalLight.Specular * directionalLight.Diffuse;
	}
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * directionalLight.Ambient * directionalLight.Diffuse;

	//Result
	vec3 result = (diffuse + ambient + specular);
	return result;
}

vec3 CalculatePointLight(PointLight pointLight)
{
	//const float KLin = 0.09, KSq = 0.032;
	const float KLin = 0.007, KSq = 0.0002;
	float distance = length(pointLight.Position - v_Position);
	distance *= distance / pointLight.Distance;
	float attenuation = 1.0 / (1.0 + KLin * distance +  KSq * (distance * distance));

	//Diffuse
	vec3 n_Normal = normalize(v_Normal);
	vec3 n_LightDir = normalize(pointLight.Position - v_Position);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec4 diffuseColor = texture(u_DiffuseTextures[v_DiffuseTextureIndex], g_TiledTexCoords);
	vec3 diffuse = (diff * diffuseColor.rgb) * pointLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), v_Material.Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_SpecularTextureIndex], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * pointLight.Specular * pointLight.Diffuse;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * pointLight.Ambient * pointLight.Diffuse;

	//Result
	vec3 result = attenuation*(diffuse + ambient + specular);
	return result;
}
