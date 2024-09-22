#define EG_NO_TEXTURES
#include "defines.h"
#include "pipeline_layout.h"

#define VERTEX_COUNT 36

vec3 s_Positions[VERTEX_COUNT] = vec3[](
    // back face
    vec3(-1.0f, -1.0f, -1.0f), // bottom-left
    vec3( 1.0f, -1.0f, -1.0f), // bottom-right         
    vec3( 1.0f,  1.0f, -1.0f), // top-right
    vec3( 1.0f,  1.0f, -1.0f), // top-right
    vec3(-1.0f,  1.0f, -1.0f), // top-left
    vec3(-1.0f, -1.0f, -1.0f), // bottom-left
    // front face
    vec3(-1.0f, -1.0f,  1.0f), // bottom-left
    vec3( 1.0f,  1.0f,  1.0f), // top-right
    vec3( 1.0f, -1.0f,  1.0f), // bottom-right
    vec3( 1.0f,  1.0f,  1.0f), // top-right
    vec3(-1.0f, -1.0f,  1.0f), // bottom-left
    vec3(-1.0f,  1.0f,  1.0f), // top-left
    // left face
    vec3(-1.0f,  1.0f,  1.0f), // top-right
    vec3(-1.0f, -1.0f, -1.0f), // bottom-left
    vec3(-1.0f,  1.0f, -1.0f), // top-left
    vec3(-1.0f, -1.0f, -1.0f), // bottom-left
    vec3(-1.0f,  1.0f,  1.0f), // top-right
    vec3(-1.0f, -1.0f,  1.0f), // bottom-right
    // right face
    vec3(1.0f,  1.0f,  1.0f), // top-left
    vec3(1.0f,  1.0f, -1.0f), // top-right         
    vec3(1.0f, -1.0f, -1.0f), // bottom-right
    vec3(1.0f, -1.0f, -1.0f), // bottom-right
    vec3(1.0f, -1.0f,  1.0f), // bottom-left     
    vec3(1.0f,  1.0f,  1.0f), // top-left
    // bottom face
    vec3(-1.0f, -1.0f, -1.0f), // top-right
    vec3( 1.0f, -1.0f,  1.0f), // bottom-left
    vec3( 1.0f, -1.0f, -1.0f), // top-left
    vec3( 1.0f, -1.0f,  1.0f), // bottom-left
    vec3(-1.0f, -1.0f, -1.0f), // top-right
    vec3(-1.0f, -1.0f,  1.0f), // bottom-right
    // top face
    vec3(-1.0f,  1.0f, -1.0f), // top-left
    vec3( 1.0f,  1.0f, -1.0f), // top-right     
    vec3( 1.0f,  1.0f , 1.0f), // bottom-right
    vec3( 1.0f,  1.0f,  1.0f), // bottom-right
    vec3(-1.0f,  1.0f,  1.0f), // bottom-left      
    vec3(-1.0f,  1.0f, -1.0f)  // top-left
);

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX) readonly buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) in vec2 a_AspectRatio;
layout(location = 1) in uint a_MaterialIndex;
layout(location = 2) in uint a_TransformIndex;
layout(location = 3) in uint a_EntityID;

layout(location = 0) out vec4 o_ClipPos;
layout(location = 1) flat out vec2 o_AspectRatio;
layout(location = 2) flat out uint o_MaterialIndex;
layout(location = 3) flat out uint o_TransformIndex;
layout(location = 4) flat out uint o_EntityID;

void main()
{
    const mat4 decalVP = inverse(g_Transforms[a_TransformIndex]);

    const vec4 worldPos = decalVP * vec4(s_Positions[gl_VertexIndex], 1.0);
    gl_Position = g_ViewProj * worldPos;

    o_ClipPos = gl_Position;
    o_AspectRatio = a_AspectRatio;
    o_MaterialIndex = a_MaterialIndex;
    o_TransformIndex = a_TransformIndex;
    o_EntityID = a_EntityID;
}
