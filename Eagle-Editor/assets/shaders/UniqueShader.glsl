#type vertex
#version 450

layout(location = 0) in vec3    a_Position;
layout(location = 1) in vec3    a_Normal;
layout(location = 2) in vec2    a_TexCoord;
layout(location = 3) in int     a_EntityID;
layout(location = 4) in int     a_DiffuseTextureIndex;
layout(location = 5) in int     a_SpecularTextureIndex;
layout(location = 6) in float   a_TilingFactor;

//Material
layout(location = 7) in float  a_MaterialShininess;

uniform mat4 u_ViewProjection;

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
	v_Normal = a_Normal;
	v_TexCoord = a_TexCoord;
	v_EntityID = a_EntityID;
	v_DiffuseTextureIndex = a_DiffuseTextureIndex;
	v_SpecularTextureIndex = a_SpecularTextureIndex;
	v_TilingFactor = a_TilingFactor;

	//Material
	v_Material.Shininess = a_MaterialShininess;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 invertedColor;
layout (location = 2) out int entityID;

in vec3  v_Position;
in vec3  v_Normal;
in vec2  v_TexCoord;
flat in int	 v_DiffuseTextureIndex;
flat in int	 v_SpecularTextureIndex;
flat in int	 v_EntityID;
in float v_TilingFactor;

struct PointLight
{
	vec3 Position;

	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	float Distance;
};

struct DirectionalLight
{
	vec3 Direction;

	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
};

struct SpotLight
{
	vec3 Position;
	vec3 Direction;

	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	float InnerCutOffAngle;
	float OuterCutOffAngle;
};

in v_MATERIAL
{
	float Shininess;
}v_Material;

#define MAXPOINTLIGHTS 4
#define MAXSPOTLIGHTS 4

uniform vec3 u_ViewPos;
uniform sampler2D u_DiffuseTextures[16];
uniform sampler2D u_SpecularTextures[16];
uniform PointLight u_PointLights[MAXPOINTLIGHTS];
uniform DirectionalLight u_DirectionalLight;
uniform SpotLight u_SpotLights[MAXSPOTLIGHTS];
uniform int u_PointLightsSize;
uniform int u_SpotLightsSize;
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
	color = vec4(pointLightsResult + directionalLightResult + spotLightsResult + skyboxLight, diffuseAlpha);

	//Other stuff
	invertedColor = vec4(vec3(1.0) - color.rgb, color.a);
	entityID = v_EntityID;
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
	vec3 reflectDir = reflect(-n_LightDir, n_Normal);
	float specCoef = pow(max(dot(viewDir, reflectDir), 0.0), v_Material.Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_SpecularTextureIndex], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * spotLight.Specular;

	vec3 ambient = diffuseColor.rgb * spotLight.Ambient;

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
	vec4 diffuseColor = texture(u_DiffuseTextures[v_DiffuseTextureIndex], g_TiledTexCoords);
	vec3 diffuse = (diff * diffuseColor.rgb) * directionalLight.Diffuse;

	//Specular
	vec3 viewDir = normalize(u_ViewPos - v_Position);
	vec3 reflectDir = reflect(-n_LightDir, n_Normal);
	float specCoef = pow(max(dot(viewDir, reflectDir), 0.0), v_Material.Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_SpecularTextureIndex], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * directionalLight.Specular;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * directionalLight.Ambient;

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
	vec3 reflectDir = reflect(-n_LightDir, n_Normal);
	float specCoef = pow(max(dot(viewDir, reflectDir), 0.0), v_Material.Shininess);
	vec4 specularColor = texture(u_SpecularTextures[v_SpecularTextureIndex], g_TiledTexCoords);
	vec3 specular = specularColor.rgb * specCoef * pointLight.Specular;
	
	//Ambient
	vec3 ambient = diffuseColor.rgb * pointLight.Ambient;

	//Result
	vec3 result = attenuation*(diffuse + ambient + specular);
	return result;
}
