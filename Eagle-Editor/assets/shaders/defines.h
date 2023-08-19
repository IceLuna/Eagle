#ifndef EG_SHADER_DEFINES
#define EG_SHADER_DEFINES

//----------------------------
#define EG_MAX_TEXTURES 1024 // TODO: Replace with dynamic array and move this to size to C++ so we can resize it if we exceed max number.
#define EG_MAX_LIGHT_SHADOW_MAPS 1024 // TODO: Replace with dynamic array and move this to size to C++ so we can resize it if we exceed max number.
#define EG_INVALID_TEXTURE_INDEX 0 // Must be 0

#ifndef EG_CASCADES_COUNT
#define EG_CASCADES_COUNT 4 // After changing this, projections need to be adjusted. The same defined in Camera.h
#endif
//----------------------------

#define EG_PERSISTENT_SET 0

#define EG_BINDING_TEXTURES  0
#define EG_BINDING_MATERIALS 1
#define EG_BINDING_MAX       2
//----------------------------

#define EG_SCENE_SET 1

#define EG_BINDING_POINT_LIGHTS                     0
#define EG_BINDING_SPOT_LIGHTS                      1
#define EG_BINDING_DIRECTIONAL_LIGHT                2
#define EG_BINDING_ALBEDO_ROUGHNESS_TEXTURE         3
#define EG_BINDING_GEOMETRY_SHADING_NORMALS_TEXTURE 4
#define EG_BINDING_EMISSIVE_TEXTURE                 5
#define EG_BINDING_DEPTH_TEXTURE                    6
#define EG_BINDING_MATERIAL_DATA_TEXTURE            7
#define EG_BINDING_IRRADIANCE_MAP                   8
#define EG_BINDING_PREFILTER_MAP                    9
#define EG_BINDING_BRDF_LUT                         10
#define EG_BINDING_CAMERA_VIEW                      11
#define EG_BINDING_SM_DISTRIBUTION                  12
#define EG_BINDING_SSAO                             13
#define EG_BINDING_CSM_SHADOW_MAPS                  14
#define EG_BINDING_SM_POINT_LIGHT                   EG_BINDING_CSM_SHADOW_MAPS + EG_CASCADES_COUNT
#define EG_BINDING_SM_SPOT_LIGHT                    EG_BINDING_SM_POINT_LIGHT + EG_MAX_LIGHT_SHADOW_MAPS

#define EG_SM_DISTRIBUTION_TEXTURE_SIZE 16
#define EG_SM_DISTRIBUTION_FILTER_SIZE 8
#define EG_SM_DISTRIBUTION_RANDOM_RADIUS 2.f

#define EG_POINT_LIGHT_NEAR 0.01f
#define EG_POINT_LIGHT_FAR  50.f // TODO: Make it customizable

#define EG_PI     3.1415926535
#define EG_INV_PI 0.3183098861
#define EG_2PI (EG_PI * 2.f)
#define EG_HALF_PI (EG_PI * 0.5f)

#define FLT_EPSILON 1.192092896e-07F // smallest such that 1.0+FLT_EPSILON != 1.0
#define EG_FLT_SMALL 0.001f
#define EG_MIN_ROUGHNESS 0.04f
#define EG_BASE_REFLECTIVITY 0.04f
#define EG_OPACITY_MASK_THRESHOLD 0.5f

#define EG_DEFAULT_ROUGHNESS 0.5f
#define EG_DEFAULT_AO 1.f

#define IS_ZERO(x) (dot(x, x) < FLT_EPSILON)
#define NOT_ZERO(x) (!IS_ZERO(x))

#define IS_ONE(x) (abs(1.f - dot(x,x)) < 1e-3f)
#define NOT_ONE(x) (!IS_ONE(x))

#endif
