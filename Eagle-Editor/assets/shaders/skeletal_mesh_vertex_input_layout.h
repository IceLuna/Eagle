#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoords;
layout(location = 4) in f16vec4 a_Weights;
layout(location = 5) in u16vec4 a_BoneIDs;

layout(location = 6) in uvec3 a_PerInstanceData; // .x = TransformIndex (highest bit is flag whether it receives decals); .y = MaterialIndex; .z = ObjectID
