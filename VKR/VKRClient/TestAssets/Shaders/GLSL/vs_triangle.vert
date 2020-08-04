#version 450
#extension GL_ARB_separate_shader_objects : enable

vec2 positions[3] = vec2[]
(
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec4 colors[3] = vec4[]
(
    vec4(1.0f, 0.0f, 0.0f, 1.0f),
    vec4(0.0f, 1.0f, 0.0f, 1.0f),
    vec4(0.0f, 0.0f, 1.0f, 1.0f)
);

struct VS_OUT
{
    vec4 vtxColor;
};

layout (location = 0) out VS_OUT vsOutput;

void main() 
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.01, 1.0);
    vsOutput.vtxColor = colors[gl_VertexIndex];
}