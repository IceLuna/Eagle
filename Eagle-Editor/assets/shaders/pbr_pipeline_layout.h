#include "defines.h"
#include "common_structures.h"

layout(set = EG_SCENE_SET, binding = EG_BINDING_POINT_LIGHTS)
buffer PointLightsBuffer
{
	PointLight g_PointLights[];
};

layout(set = EG_SCENE_SET, binding = EG_BINDING_SPOT_LIGHTS)
buffer SpotLightsBuffer
{
	SpotLight g_SpotLights[];
};

layout(set = EG_SCENE_SET, binding = EG_BINDING_DIRECTIONAL_LIGHT)
buffer DirectionalLightBuffer
{
	DirectionalLight g_DirectionalLight;
};

layout(set = EG_SCENE_SET, binding = EG_BINDING_CAMERA_VIEW)
uniform CameraView
{
	mat4 g_CameraView;
};

layout(set = EG_SCENE_SET, binding = EG_BINDING_ALBEDO_TEXTURE)        uniform sampler2D       g_AlbedoTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_NORMAL_TEXTURE)        uniform sampler2D       g_NormalTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_DEPTH_TEXTURE)         uniform sampler2D       g_DepthTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_MATERIAL_DATA_TEXTURE) uniform sampler2D       g_MaterialDataTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_IRRADIANCE_MAP)        uniform samplerCube     g_IrradianceMap;
layout(set = EG_SCENE_SET, binding = EG_BINDING_PREFILTER_MAP)         uniform samplerCube     g_PrefilterMap;
layout(set = EG_SCENE_SET, binding = EG_BINDING_BRDF_LUT)              uniform sampler2D       g_BRDFLUT;
layout(set = EG_SCENE_SET, binding = EG_BINDING_CSM_SHADOW_MAPS)       uniform sampler2DShadow  g_DirShadowMaps[EG_CASCADES_COUNT];
