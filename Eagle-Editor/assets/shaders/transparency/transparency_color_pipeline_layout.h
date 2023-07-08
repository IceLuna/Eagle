#ifndef EG_TRANSPARENCY_COLOR_PIPELINE_LAYOUT
#define EG_TRANSPARENCY_COLOR_PIPELINE_LAYOUT

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

layout(set = EG_SCENE_SET, binding = EG_BINDING_DIRECTIONAL_LIGHT + 1)                                                    uniform samplerCube g_IrradianceMap;
layout(set = EG_SCENE_SET, binding = EG_BINDING_DIRECTIONAL_LIGHT + 2)                                                    uniform samplerCube g_PrefilterMap;
layout(set = EG_SCENE_SET, binding = EG_BINDING_DIRECTIONAL_LIGHT + 3)                                                    uniform sampler2D   g_BRDFLUT;
layout(set = EG_SCENE_SET, binding = EG_BINDING_DIRECTIONAL_LIGHT + 4)                                                    uniform sampler3D   g_SmDistribution;
layout(set = EG_SCENE_SET, binding = EG_BINDING_DIRECTIONAL_LIGHT + 5)                                                    uniform sampler2D   g_DirShadowMaps[EG_CASCADES_COUNT];
layout(set = EG_SCENE_SET, binding = (EG_BINDING_DIRECTIONAL_LIGHT + 5) + EG_CASCADES_COUNT)                              uniform samplerCube g_PointShadowMaps[EG_MAX_LIGHT_SHADOW_MAPS];
layout(set = EG_SCENE_SET, binding = ((EG_BINDING_DIRECTIONAL_LIGHT + 5) + EG_CASCADES_COUNT) + EG_MAX_LIGHT_SHADOW_MAPS) uniform sampler2D   g_SpotShadowMaps[EG_MAX_LIGHT_SHADOW_MAPS];

#endif
