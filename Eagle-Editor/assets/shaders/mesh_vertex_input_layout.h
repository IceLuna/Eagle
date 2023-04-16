layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoords;

layout(location = 4) in uvec3 a_PerInstanceData; // .x = TransformIndex; .y = MaterialIndex; .z = ObjectID
