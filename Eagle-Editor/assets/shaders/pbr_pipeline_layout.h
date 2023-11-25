#ifndef EG_PBR_PIPELINE_LAYOUT
#define EG_PBR_PIPELINE_LAYOUT

#include "defines.h"
#include "common_structures.h"

layout(set = EG_SCENE_SET, binding = EG_BINDING_POINT_LIGHTS)
readonly buffer PointLightsBuffer
{
	PointLight g_PointLights[];
};

layout(set = EG_SCENE_SET, binding = EG_BINDING_SPOT_LIGHTS)
readonly buffer SpotLightsBuffer
{
	SpotLight g_SpotLights[];
};

layout(set = EG_SCENE_SET, binding = EG_BINDING_DIRECTIONAL_LIGHT)
readonly buffer DirectionalLightBuffer
{
	DirectionalLight g_DirectionalLight;
};

layout(set = EG_SCENE_SET, binding = EG_BINDING_CAMERA_VIEW)
uniform CameraView
{
	mat4 g_CameraView;
};

layout(set = EG_SCENE_SET, binding = EG_BINDING_ALBEDO_ROUGHNESS_TEXTURE)         uniform sampler2D       g_AlbedoTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_GEOMETRY_SHADING_NORMALS_TEXTURE) uniform sampler2D       g_GeometryShadingNormalsTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_EMISSIVE_TEXTURE)                 uniform sampler2D       g_EmissiveTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_DEPTH_TEXTURE)                    uniform sampler2D       g_DepthTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_MATERIAL_DATA_TEXTURE)            uniform sampler2D       g_MaterialDataTexture;
layout(set = EG_SCENE_SET, binding = EG_BINDING_IRRADIANCE_MAP)                   uniform samplerCube     g_IrradianceMap;
layout(set = EG_SCENE_SET, binding = EG_BINDING_PREFILTER_MAP)                    uniform samplerCube     g_PrefilterMap;
layout(set = EG_SCENE_SET, binding = EG_BINDING_BRDF_LUT)                         uniform sampler2D       g_BRDFLUT;
layout(set = EG_SCENE_SET, binding = EG_BINDING_SM_DISTRIBUTION)                  uniform sampler3D       g_SmDistribution;
layout(set = EG_SCENE_SET, binding = EG_BINDING_SSAO)                             uniform sampler2D       g_SSAO;
layout(set = EG_SCENE_SET, binding = EG_BINDING_CSM_SHADOW_MAPS)                  uniform sampler2D       g_DirShadowMaps[EG_CASCADES_COUNT];
layout(set = 2, binding = 0)                                                      uniform samplerCube     g_PointShadowMaps[];
layout(set = 3, binding = 0)                                                      uniform sampler2D       g_SpotShadowMaps[];
#ifdef EG_TRANSLUCENT_SHADOWS
layout(set = EG_SCENE_SET, binding = EG_BINDING_CSMC_SHADOW_MAPS)                 uniform sampler2D       g_DirShadowMapsColored[EG_CASCADES_COUNT];
layout(set = 4, binding = 0)                                                      uniform samplerCube     g_PointShadowMapsColored[];
layout(set = 5, binding = 0)                                                      uniform sampler2D       g_SpotShadowMapsColored[];
#endif

#endif
