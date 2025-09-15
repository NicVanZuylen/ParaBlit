#extension GL_EXT_ray_tracing : require
#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "pb_common.h"

#define SHADOW_PAYLOAD_LOCATION 0
#define REFLECTION_PAYLOAD_LOCATION 1
#define CAMERA_RAY_PAYLOAD_LOCATION 2

#define DEFINE_WORLD_CONSTANTS(name)                        \
layout(set = 1, binding = 0) uniform WorldConstantsBuffer   \
{                                                           \
    vec3 sunDirection;                                      \
    float sunDistance;                                      \
    vec3 sunColor;                                          \
    float sunIntensity;                                     \
    float sunRadius;                                        \
    float randSeed;                                         \
    uint checkerboardIndex;                                 \
    uint blueNoiseTextureLayerIndex;                        \
} name[]                                                    \

#define DEFINE_PATH_TRACING_BINDINGS                        \
layout(push_constant) uniform Bindings                      \
{                                                           \
    uint viewConstantsIndex;                                \
    uint worldConstantsIndex;                               \
    uint objPoolInstanceIndexBufferIndex;                   \
    uint meshLibraryBufferIndex;                            \
                                                            \
    /* G Buffers */                                         \
    uint normalGBufferIndex;                                \
    uint specAndRoughTextureIndex;                          \
    uint depthBufferIndex;                                  \
    uint motionVectorTextureIndex;                          \
                                                            \
    uint noiseTexturesIndex;                                \
    uint skyCubeTextureIndex;                               \
    uint skySamplerIndex;                                   \
                                                            \
    /* Shadow inputs/outputs */                             \
    uint prevShadowAccumTextureIndex;                       \
    uint shadowAccumTextureIndex;                           \
    uint outShadowTextureIndex;                             \
    uint outPenumbraTextureIndex;                           \
                                                            \
    /* Reflection inputs/outputs */                         \
    uint prevReflectionAccumTextureIndex;                   \
    uint reflectionAccumTextureIndex;                       \
    uint outReflectionIndex;                                \
} PB_BINDINGS_NAME                                          \

struct AccelerationStructureInstance
{
    float xX;
    float xY;
    float xZ;
    float posX;
    float yX;
    float yY;
    float yZ;
    float posY;
    float zX;
    float zY;
    float zZ;
    float posZ;

    uint instanceIndexAndMask; // 0xFF000000 = mask, 0x00FFFFFF = instance index
    uint sbtRecordOffsetAndFlags; // 0xFF000000 = flags, 0x00FFFFFF = sbt record offset
    uint64_t accelerationStructurePointer; // vec2 of 32-bit uints representing the 64-bit device address of the acceleration structure this instance is referencing.
};

struct CameraRayPayload
{
    vec2 penumbra;
    float shadow;
    float pad;
};

struct ShadowRayPayload
{
    uint missCount;
    float hitDistance;
};

struct ReflectionRayPayload
{
    vec3 color;
};

PB_DEFINE_TEXTURE_ARRAY_BINDINGS;

const float PHI = 1.61803398874989484820459;

// ********************************************************************************************************************************
// ********************************************************************************************************************************
float Rand(vec2 co, float seed)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453 * seed * PHI);
}
// ********************************************************************************************************************************
// ********************************************************************************************************************************
vec3 GetConeSampleRand(in const vec2 uv, inout float randSeed, in const vec3 targetPosition, in const vec3 direction, in const float radius)
{
    vec3 perpX = cross(direction, vec3(0.0, 1.0, 0.0));
    if(perpX.x == 0.0 && perpX.y == 0.0 && perpX.z == 0.0)
    {
        perpX.x = 1.0;
    }

    vec3 perpY = cross(direction, perpX);

    float x = Rand(uv, randSeed);
    randSeed = x;
    float y = Rand(uv * x, randSeed);
    randSeed = y;

    vec3 target = targetPosition + (perpX * x * radius) + (perpY * y * radius);
    return -normalize(target);
}
// ********************************************************************************************************************************
// ********************************************************************************************************************************
vec3 GetConeSampleBlueNoise(in const uvec3 launchID, in const vec3 targetPosition, in const vec3 direction, in const float radius, in const uint noiseTextureArrayIndex, in const uint noiseTextureLayerIndex)
{
    ivec3 samplePos = ivec3(launchID.x % 256, launchID.y % 256, noiseTextureLayerIndex);
    vec2 blueNoise = texelFetch(PB_TEXTURE_ARRAY_BINDINGS_NAME[noiseTextureArrayIndex], samplePos, 0).xy;

    vec3 perpX = cross(direction, vec3(0.0, 1.0, 0.0));
    if(perpX.x == 0.0 && perpX.y == 0.0 && perpX.z == 0.0)
    {
        perpX.x = 1.0;
    }

    vec3 perpY = cross(direction, perpX);

    float x = blueNoise.x;
    float y = blueNoise.y;

    vec3 target = targetPosition + (perpX * x * radius) + (perpY * y * radius);
    return -normalize(target);
}
// ********************************************************************************************************************************
// ********************************************************************************************************************************