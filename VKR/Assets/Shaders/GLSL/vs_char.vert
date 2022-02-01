#version 450
#include "Common/pb_common.h"
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

layout(push_constant) uniform Bindings
{
    uint textConstantsIndex;
    uint charInstanceIndex;
    uint fontDataIndex;
    uint fontSamplerIdx;
} PB_BINDINGS_NAME;

layout(set = 1, binding = 0) uniform TextConstants
{
    vec2 renderDimensions;
} textConstants[];
#define CONSTANTS PB_UBO(textConstants, textConstantsIndex)

struct CharInstance
{
    uint packedIndices;     // Packed char and font data indices.
    uint packedPosition;    // Packed half-precision XY position values.
};

layout(set = 0, binding = 2) buffer CharInstanceBuffer
{
    CharInstance instances[];
} charInstanceBuffers[];

struct FontData
{
    vec4 fontColor;
    uint fontTextureIdx;
    uint fontGlyphBufferIdx;
    vec2 pad;
};

layout(set = 0, binding = 2) buffer FontDataBuffer
{
    FontData data[];
} fontDataBuffers[];

layout(set = 0, binding = 2) buffer GlyphDataBuffer
{
    vec4 data[];
} glyphDataBuffers[];

layout (location = 0) out VS_OUT
{
    vec4 fontColor;
    vec2 texCoord;
    flat uint fontTexIdx;
    float pad;
} vsOutput;

void main() 
{
    CharInstance instance = charInstanceBuffers[PB_BINDINGS_NAME.charInstanceIndex].instances[gl_InstanceIndex];
    uint charIdx = instance.packedIndices & 0xFFFF;
    uint fontDataIdx = instance.packedIndices >> 16;
    vec2 offset = unpackHalf2x16(instance.packedPosition) / CONSTANTS.renderDimensions;

    FontData fontData = fontDataBuffers[PB_BINDINGS_NAME.fontDataIndex].data[fontDataIdx];

    // Behaves as a rectangle in texture coordinate space.
    vec4 glyph = glyphDataBuffers[fontData.fontGlyphBufferIdx].data[charIdx];

    vec2 scaledPosition = positions[gl_VertexIndex] * glyph.zw;

    gl_Position = vec4(scaledPosition + offset, 0.01, 1.0);
    vsOutput.fontColor = fontData.fontColor;
    vsOutput.texCoord = glyph.xy + (((positions[gl_VertexIndex] + 1.0) * 0.5) * glyph.zw);
    vsOutput.fontTexIdx = fontData.fontTextureIdx;
}