#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
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

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;
flat out int v_Index;

void main()
{
	gl_Position = u_Projection * u_View * u_Models[a_Index] * vec4(a_Position, 1.0);
	
	v_Position = (u_Models[a_Index] * vec4(a_Position, 1.0)).xyz;

	v_Normal   = mat3(transpose(inverse(u_Models[a_Index]))) * a_Normal;
	v_TexCoord = a_TexCoord;
	v_Index = a_Index;
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;
layout (location = 2) out int  entityID;

in vec3  v_Position;
in vec3  v_Normal;
in vec2  v_TexCoord;
flat in int v_Index;

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
	vec3 Direction; //16 0

	vec3 Ambient; //16 16
	vec3 Diffuse; //16 32
	vec3 Specular;//16 48
}; //Total Size = 64

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
	DirectionalLight u_DirectionalLight; //64
	SpotLight u_SpotLights[MAXSPOTLIGHTS]; //96 * MAXSPOTLIGHTS
	PointLight u_PointLights[MAXPOINTLIGHTS]; //64 * MAXPOINTLIGHTS
	int u_PointLightsSize; //4
	int u_SpotLightsSize; //4
}; //Total Size = 720

uniform vec3 u_ViewPos;

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
	BatchData u_BatchData[BATCH_SIZE];
}; // Total size = 1680.

uniform sampler2D u_DiffuseTextures[BATCH_SIZE];
uniform sampler2D u_SpecularTextures[BATCH_SIZE];

uniform samplerCube u_Skybox;
uniform int u_SkyboxEnabled;

vec3 CalculatePointLight(PointLight pointLight);
vec3 CalculateDirectionalLight(DirectionalLight directionalLight);
vec3 CalculateSpotLight(SpotLight spotLight);
vec3 CalculateSkyboxLight();

vec2 g_TiledTexCoords;

void main()
{
	vec3 pointLightsResult = vec3(0.0);
	vec3 spotLightsResult = vec3(0.0);
	g_TiledTexCoords = v_TexCoord * u_BatchData[v_Index].TilingFactor;

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

	double diffuseAlpha = texture(u_DiffuseTextures[v_Index], g_TiledTexCoords).a;
	color = vec4(pointLightsResult + directionalLightResult + spotLightsResult + skyboxLight, diffuseAlpha);

	//Other stuff
	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
	entityID = u_BatchData[v_Index].EntityID;
}

vec3 CalculateSkyboxLight()
{
	vec3 viewDir = normalize(v_Position - u_ViewPos);
	vec3 R = reflect(viewDir, normalize(v_Normal));
	vec3 result = texture(u_Skybox, R).rgb;

	vec3 specularColor = texture(u_SpecularTextures[v_Index], g_TiledTexCoords).rgb;

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
	vec4 diffuseColor = texture(u_DiffuseTextures[v_Index], g_TiledTexCoords);
	vec3 diffuse = (diff * diffuseColor.rgb) * spotLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), u_BatchData[v_Index].Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_Index], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * spotLight.Specular * spotLight.Diffuse;

	vec3 ambient = diffuseColor.rgb * spotLight.Ambient * spotLight.Diffuse;

	//Result
	vec3 result = intensity * (diffuse + specular + ambient);
	return result;
}

vec3 CalculateDirectionalLight(DirectionalLight directionalLight)
{
	//Diffuse
	vec3 n_Normal = normalize(v_Normal);
	vec3 n_LightDir = normalize(-directionalLight.Direction);
	float diff = max(dot(n_Normal, n_LightDir), 0.0);
	vec4 diffuseColor = texture(u_DiffuseTextures[v_Index], g_TiledTexCoords);
	vec3 diffuse = (diff * diffuseColor.rgb) * directionalLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), u_BatchData[v_Index].Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_Index], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * directionalLight.Specular * directionalLight.Diffuse;
	
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
	vec4 diffuseColor = texture(u_DiffuseTextures[v_Index], g_TiledTexCoords);
	vec3 diffuse = (diff * diffuseColor.rgb) * pointLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 halfwayDir = normalize(n_LightDir + viewDir);
	float specCoef = pow(max(dot(n_Normal, halfwayDir), 0.0), u_BatchData[v_Index].Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_Index], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * pointLight.Specular * pointLight.Diffuse;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * pointLight.Ambient * pointLight.Diffuse;

	//Result
	vec3 result = attenuation*(diffuse + ambient + specular);
	return result;
}
