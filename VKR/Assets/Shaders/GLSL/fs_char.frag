#version 450
#include "Common/pb_common.h"

layout(push_constant) uniform Bindings
{
    uint textConstantsIndex;
    uint charInstanceIndex;
    uint fontDataIndex;
    uint fontSamplerIdx;
} PB_BINDINGS_NAME;

layout(location = 0) in VS_IN
{
    vec4 fontColor;
    vec2 texCoord;
    flat uint fontTexIdx;
    float pad;
} vsInput;

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout(location = 0) out vec4 outColor;

void main()
{
    float fontMask = texture
    (
        sampler2D
        (
            pb_textures[nonuniformEXT(vsInput.fontTexIdx)],
            PB_SAMPLER(fontSamplerIdx)
        ),
        vsInput.texCoord
    ).r;

    outColor = vec4(vsInput.fontColor.rgb, fontMask);
}