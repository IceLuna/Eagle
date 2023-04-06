#define VERTEX_COUNT 36

vec3 positions[VERTEX_COUNT] = vec3[](
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

layout(location = 0) out vec3 o_Pos;

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

void main()
{
    const vec4 pos = vec4(positions[gl_VertexIndex], 1.0);
    const vec4 clipPos = g_ViewProj * pos;

    gl_Position = clipPos.xyww;

    o_Pos = pos.xyz;
}
