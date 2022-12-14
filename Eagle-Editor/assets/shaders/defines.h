#define EG_PERSISTENT_SET 0

#define EG_BINDING_TEXTURE  0
#define EG_BINDING_MATERIAL 1
#define EG_BINDING_MAX      2
//----------------------------

#define EG_PBR_SET 1

#define EG_BINDING_POINT_LIGHTS          0
#define EG_BINDING_SPOT_LIGHTS           1
#define EG_BINDING_DIRECTIONAL_LIGHT     2
#define EG_BINDING_ALBEDO_TEXTURE        3
#define EG_BINDING_NORMAL_TEXTURE        4
#define EG_BINDING_DEPTH_TEXTURE         5
#define EG_BINDING_MATERIAL_DATA_TEXTURE 6
#define EG_BINDING_IRRADIANCE_MAP        7
#define EG_BINDING_PREFILTER_MAP         8
#define EG_BINDING_BRDF_LUT              9

//----------------------------
#define MAX_TEXTURES 1024 // TODO: Replace with dynamic array and move this to size to C++ so we can resize it if we exceed max number.
#define INVALID_TEXTURE_INDEX 0 // Must be 0

#define PI 3.1415926535
