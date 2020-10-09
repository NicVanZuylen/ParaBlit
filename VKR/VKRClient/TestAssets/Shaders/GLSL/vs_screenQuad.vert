#version 450
#extension GL_ARB_separate_shader_objects : enable

vec2 positions[6] = vec2[]
(
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(-1.0, -1.0)
);

struct VS_OUT
{
    vec2 texCoord;
    vec2 pad0;
};

layout (location = 0) out VS_OUT vsOutput;

void main() 
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.01, 1.0);
    vsOutput.texCoord = (positions[gl_VertexIndex] + 1) * 0.5;
}