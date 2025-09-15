#extension GL_EXT_nonuniform_qualifier : require

#ifndef PB_COMMON_H
#define PB_COMMON_H

// --------------------------------------------------------------------------------------------------------
// Macros
// --------------------------------------------------------------------------------------------------------

// The name that should be assigned to a push constants buffer used for bindings.
#define PB_BINDINGS_NAME pb_bindings

// Names of resource array declarations.
#define PB_TEXTURE_BINDINGS_NAME pb_textures
#define PB_TEXTURE_CUBE_BINDINGS_NAME pb_textures_cube
#define PB_TEXTURE_ARRAY_BINDINGS_NAME pb_textures_array
#define PB_SAMPLER_BINDINGS_NAME pb_samplers
#define PB_STORAGE_IMAGE_BINDINGS_NAME pb_storageImages

// Define texture binding array.
#define PB_DEFINE_TEXTURE_BINDINGS layout(set = 0, binding = 0) uniform texture2D PB_TEXTURE_BINDINGS_NAME[]
#define PB_DEFINE_TEXTURE_CUBE_BINDINGS layout(set = 0, binding = 0) uniform textureCube PB_TEXTURE_CUBE_BINDINGS_NAME[]
#define PB_DEFINE_TEXTURE_ARRAY_BINDINGS layout(set = 0, binding = 0) uniform texture2DArray PB_TEXTURE_ARRAY_BINDINGS_NAME[]

// Define sampler binding array.
#define PB_DEFINE_SAMPLER_BINDINGS layout(set = 0, binding = 1) uniform sampler PB_SAMPLER_BINDINGS_NAME[]
// Define storage image binding array with the provided qualifiers.
#define PB_DEFINE_STORAGE_IMAGE_BINDINGS(qualifiers, format) layout(set = 0, binding = 3, format) uniform qualifiers image2D PB_STORAGE_IMAGE_BINDINGS_NAME[]
#define PB_DEFINE_STORAGE_IMAGE_BINDINGS_WITH_NAME(name, qualifiers, format) layout(set = 0, binding = 3, format) uniform qualifiers image2D name[]

// Shorthand for accessing a texture using a non uniform binding index.
#define PB_TEXTURE_NU(index) PB_TEXTURE_BINDINGS_NAME[nonuniformEXT(index)]
// Shorthand for accessing a texture using its binding index.
#define PB_TEXTURE(index) PB_TEXTURE_BINDINGS_NAME[PB_BINDINGS_NAME.index]
// Shorthand for accessing a texture cube using a non uniform binding index.
#define PB_TEXTURE_CUBE_NU(index) PB_TEXTURE_CUBE_BINDINGS_NAME[nonuniformEXT(index)]
// Shorthand for accessing a texture cube using its binding index.
#define PB_TEXTURE_CUBE(index) PB_TEXTURE_CUBE_BINDINGS_NAME[PB_BINDINGS_NAME.index]
// Shorthand for accessing a texture array using its binding index.
#define PB_TEXTURE_ARRAY(index) PB_TEXTURE_ARRAY_BINDINGS_NAME[PB_BINDINGS_NAME.index]
// Shorthand for accessing a sampler using a non uniform binding index.
#define PB_SAMPLER_NU(index) PB_SAMPLER_BINDINGS_NAME[nonuniformEXT(index)]
// Shorthand for accessing a sampler using its binding index.
#define PB_SAMPLER(index) PB_SAMPLER_BINDINGS_NAME[PB_BINDINGS_NAME.index]
// Shorthand for accessing a storage image using a non uniform binding index.
#define PB_STORAGE_IMAGE_NU(index) PB_STORAGE_IMAGE_BINDINGS_NAME[nonuniformEXT(index)]
// Shorthand for accessing a storage image using its binding index.
#define PB_STORAGE_IMAGE(index) PB_STORAGE_IMAGE_BINDINGS_NAME[PB_BINDINGS_NAME.index]
#define PB_STORAGE_IMAGE_NAME(name, index) name[PB_BINDINGS_NAME.index]
// Shorthand for accessing a uniform buffer using a non uniform binding index.
#define PB_UBO_NU(name, index) name[nonuniformEXT(index)]
// Shorthand for accessing a uniform buffer using its binding index.
#define PB_UBO(name, index) name[PB_BINDINGS_NAME.index]
// Shorthand for accessing a storage buffer using a non uniform binding index.
#define PB_STORAGE_BUFFER_NU(name, index) name[nonuniformEXT(index)]
// Shorthand for accessing a storage buffer using its binding index.
#define PB_STORAGE_BUFFER(name, index) name[PB_BINDINGS_NAME.index]

// Shorthand for building a combined image sampler using the binding indices of a texture and sampler.
#define PB_BUILD_SAMPLER(t, s) sampler2D(PB_TEXTURE(t), PB_SAMPLER(s))
#define PB_BUILD_SAMPLER_CUBE(t, s) samplerCube(PB_TEXTURE_CUBE(t), PB_SAMPLER(s))

// --------------------------------------------------------------------------------------------------------
// Data Structures
// --------------------------------------------------------------------------------------------------------
struct IndirectDrawMeshTasksParams
{
	uint workgroupX;
    uint workgroupY;
    uint workgroupZ;
};

// --------------------------------------------------------------------------------------------------------
// Helper Functions
// --------------------------------------------------------------------------------------------------------

vec3 WorldPosFromDepth(float depth, vec2 texCoords, in mat4 invView, in mat4 invProj)
{
	// Convert x & y to clip space and include z.
	vec4 clipSpacePos = vec4(texCoords * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpacepos = invProj * clipSpacePos; // Transform clip space position to view space.

	// Do perspective divide
	viewSpacepos /= viewSpacepos.w;

	// Transform to worldspace from viewspace.
	vec4 worldPos = invView * viewSpacepos;

	return worldPos.xyz;
}

#endif /* PB_COMMON_H */