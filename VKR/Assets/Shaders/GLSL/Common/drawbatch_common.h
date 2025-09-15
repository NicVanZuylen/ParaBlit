#define DRAWBATCH_MAX_INSTANCES 256
#define DRAWBATCH_TASK_GROUP_SIZE 32

#define DEFINE_REQUIRED_UNIFORM_BINDINGS    \
uint cullConstantsIndex;                    \
uint viewConstIndex                         \

#define INSTANCE_TEXTURE_COUNT 8

struct VS_INSTANCE
{
    mat4 model;
    mat4 prevModel; // Model matrix from the previous frame, used for calculating motion vectors.

    uint lodData; // Same format as seen in the definition of MeshLibraryEntry in vertex_common.h
    uint samplerIndex;
    uint pad0;

    // ------------- Bindings filled by AssetStreamer -------------
    uint meshID;
    uint textureIndices[INSTANCE_TEXTURE_COUNT];
    // ------------------------------------------------------------
};

#define DEFINE_DRAWBATCH_BINDINGS       \
layout(push_constant) uniform Bindings  \
{                                       \
    DEFINE_REQUIRED_UNIFORM_BINDINGS;   \
    uint meshletRangesBufferIndex;      \
    uint drawRangeBufferIndex;          \
    uint instanceBufferIndex;           \
    uint meshLibraryBufferIndex;        \
} PB_BINDINGS_NAME                      \

//