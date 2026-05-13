#ifndef EG_SHADER_DEFINES
#define EG_SHADER_DEFINES

//----------------------------
#define EG_MAX_LIGHT_SHADOW_MAPS 1024 // TODO: Replace with dynamic array and move this to size to C++ so we can resize it if we exceed max number.
#define EG_INVALID_INDEX 0 // Must be 0
#define EG_INVALID_SHADOW_MAP (0xFFFFFFFF)

#define EG_LIGHT_CULLING_TILE_SIZE 16
#define EG_MAX_LIGHTS_PER_TILE 256
#define EG_LIGHT_BUCKET_SIZE 32
#define EG_LIGHTS_BUCKET_COUNT (EG_MAX_LIGHTS_PER_TILE / EG_LIGHT_BUCKET_SIZE)

#ifndef EG_CASCADES_COUNT
#define EG_CASCADES_COUNT 4 // After changing this, projections need to be adjusted. The same defined in Camera.h
#endif
//----------------------------

#define EG_PERSISTENT_SET 0
#define EG_MAX_SET        1
#define EG_TEXTURES_SET   EG_MAX_SET + 1

#define EG_BINDING_MATERIALS     0
#define EG_BINDING_RAW_MATERIALS 1
#define EG_BINDING_MAX           2

#define EG_BINDING_TEXTURES  0
//----------------------------

#define EG_SCENE_SET 1

#define EG_BINDING_LIGHT_MATRICES                   0
#define EG_BINDING_POINT_LIGHTS                     1
#define EG_BINDING_SPOT_LIGHTS                      2
#define EG_BINDING_POINT_LIGHT_TILE_BUCKETS         3
#define EG_BINDING_SPOT_LIGHT_TILE_BUCKETS          4
#define EG_BINDING_LIGHTS_COUNT                     5
#define EG_BINDING_DIRECTIONAL_LIGHT                6
#define EG_BINDING_ALBEDO_ROUGHNESS_TEXTURE         7
#define EG_BINDING_GEOMETRY_SHADING_NORMALS_TEXTURE 8
#define EG_BINDING_EMISSIVE_TEXTURE                 9
#define EG_BINDING_DEPTH_TEXTURE                    10
#define EG_BINDING_MATERIAL_DATA_TEXTURE            11
#define EG_BINDING_IRRADIANCE_MAP                   12
#define EG_BINDING_PREFILTER_MAP                    13
#define EG_BINDING_BRDF_LUT                         14
#define EG_BINDING_CAMERA_VIEW                      15
#define EG_BINDING_SM_DISTRIBUTION                  16
#define EG_BINDING_SSAO                             17
#define EG_BINDING_CSM_SHADOW_MAPS                  18
#define EG_BINDING_CSMC_SHADOW_MAPS                 EG_BINDING_CSM_SHADOW_MAPS + EG_CASCADES_COUNT

#define EG_SM_DISTRIBUTION_TEXTURE_SIZE 16
#define EG_SM_DISTRIBUTION_FILTER_SIZE 8
#define EG_SM_DISTRIBUTION_RANDOM_RADIUS 2.f

#define EG_POINT_LIGHT_NEAR 0.01f

#define EG_PI     3.1415926535
#define EG_INV_PI 0.3183098861
#define EG_2PI (EG_PI * 2.f)
#define EG_HALF_PI (EG_PI * 0.5f)

#define FLT_EPSILON 1.192092896e-07F // smallest such that 1.0+FLT_EPSILON != 1.0
#ifndef FLT_MIN
#define FLT_MIN 1.175494351e-38
#endif
#define FLT_MAX 3.402823466e+38F     // max value
#define EG_FLT_SMALL 0.001f
#define EG_MIN_ROUGHNESS 0.04f
#define EG_BASE_REFLECTIVITY 0.04f
#define EG_OPACITY_MASK_THRESHOLD 0.5f

#define EG_DEFAULT_ROUGHNESS 0.5f
#define EG_DEFAULT_AO 1.f

#define IS_ZERO(x) (abs(x) < FLT_EPSILON)
#define NOT_ZERO(x) (!IS_ZERO(x))

#define IS_ONE(x) (abs(1.f - x) < 1e-3f)
#define NOT_ONE(x) (!IS_ONE(x))

#define IS_EQUAL(x, y) (abs(dot(x - y, x - y)) < 1e-3f)

#define EG_SQUARE(x) ((x) * (x))

#define EG_REVERSED_DEPTH
#ifdef EG_REVERSED_DEPTH
#define EG_DEPTH_FAR 0.f
#else
#define EG_DEPTH_FAR 1.f
#endif

#define EG_RECEIVES_DECALS_MASK (1 << 31)
#define EG_CASTS_SHADOWS_MASK (1 << 31)
#define EG_FLAGS_RECEIVES_DECALS_MASK (1 << 0)

#endif
