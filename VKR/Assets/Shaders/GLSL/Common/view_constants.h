
#define DEFINE_VIEW_CONSTANTS(name)                 \
layout(set = 1, binding = 0) uniform ViewConstants  \
{                                                   \
    mat4 viewProj;                                  \
    mat4 view;                                      \
    mat4 proj;                                      \
    mat4 invView;                                   \
    mat4 invProj;                                   \
    vec4 mainFrustrumPlanes[6];                     \
    vec4 cameraPosition;                            \
    float aspectRatio;                              \
    float tanHalfFOV;                               \
} name[]                                            \

float ReconstructViewZFromDepth(float depth, in mat4 proj)
{
    float s = proj[2][2];
    float t = proj[3][2];

    // Depth in (0, 1) linear range.
    float z = t / (2 * (1.0 - depth) - 1 - s);

    return z;
}

// Technique from https://ogldev.org/www/tutorial46/tutorial46.html
// This uses inverse values from the projection matrix, to reconstruct the original view-space position from screen coords and depth (sampled from depth buffer).
vec3 ReconstructPositionFromDepth(vec2 clipCoord, float depth, float aspectRatio, float tanHalfFOV, in mat4 proj)
{
    float z = ReconstructViewZFromDepth(depth, proj);
    float x = (clipCoord.x * aspectRatio * tanHalfFOV) * z;
    float y = (clipCoord.y * tanHalfFOV) * z;

    return vec3(-x, y, z);
}