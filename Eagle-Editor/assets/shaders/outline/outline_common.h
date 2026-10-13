#ifndef EG_OUTLINE_COMMON
#define EG_OUTLINE_COMMON

#ifdef __cplusplus
using uint = uint32_t;
using mat4 = glm::mat4;
using vec4 = glm::vec4;
#endif

// Outline mask layout (R16_UInt):
//  - Bits [0; 14]: Style index + 1. 0 means that there's no outlined object at that pixel
//  - Bit 15: Set if the outlined object isn't occluded by the scene at that pixel
#define EG_OUTLINE_VISIBLE_BIT (1u << 15)
#define EG_OUTLINE_STYLE_MASK (EG_OUTLINE_VISIBLE_BIT - 1u)
#define EG_OUTLINE_MAX_STYLES EG_OUTLINE_STYLE_MASK // Style index + 1 must fit into the style bits

#ifdef EG_MATERIALS_REQUIRED
#define EG_OUTLINE_GEOMETRY_SET 1
#define EG_OUTLINE_PASS_SET 3
#else
#define EG_OUTLINE_GEOMETRY_SET 0
#define EG_OUTLINE_PASS_SET 1
#endif

struct OutlineStyle
{
    uint Color; // Packed RGBA (packUnorm4x8/unpackUnorm4x8)
    float Thickness;
    float VisibleOpacity;
    float HiddenOpacity;
};

struct OutlinePushData
{
    mat4 ViewProj;     // Note: for billboards, it's just a projection matrix since their vertices are already in view space
    uint StyleID;
    uint VertexOffset; // Skeletal meshes only. Offset of the instance's vertices in the skin cache
};

#ifdef EG_SHADER_VERTEX
layout(push_constant) uniform OutlinePushConstants
{
    OutlinePushData g_PushData;
};
#endif

#endif
