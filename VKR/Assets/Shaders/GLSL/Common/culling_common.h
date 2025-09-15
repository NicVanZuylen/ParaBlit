#define DEFINE_CULL_VIEW_CONSTANTS_UBO(name)                \
layout(set = 1, binding = 0) uniform CullingConstants       \
{                                                           \
    vec4 mainFrustrumPlanes[6];                             \
	vec3 cameraOrigin;										\
	uint isOrthographic;									\
} name[]                                                    \

#define DEFINE_CULL_VIEW_CONSTANTS_STORAGE_BUFFER(name)         \
layout(set = 0, binding = 2) readonly buffer CullingConstants   \
{                                                               \
    vec4 mainFrustrumPlanes[6];                                 \
	vec3 cameraOrigin;											\
	uint isOrthographic;										\
} name[]                                                        \

struct MeshletData
{
    vec3 center;
	float radius;

	uint normalDataXYPacked;
	uint normalDataZThetaPacked;
	uint vertOffsetCountPacked;
	uint primOffsetCountPacked;
};

// Vertices order:
// 0. vec4 botLeft;
// 1. vec4 botRight;
// 2. vec4 botBackLeft;
// 3. vec4 botBackRight;
// 4. vec4 topLeft;
// 5. vec4 topRight;
// 6. vec4 topBackLeft;
// 7. vec4 topBackRight;

// void TransformBounds(in mat4 transformMatrix, inout vec3 origin, inout vec3 extents)
// {
//     vec4 v[8];
//     v[0] = vec4(origin, 1.0);
//     v[1] = vec4(origin + vec3(extents.x, 0.0, 0.0), 1.0);
//     v[2] = vec4(origin + vec3(0.0, 0.0, extents.z), 1.0);
//     v[3] = vec4(origin + vec3(extents.x, 0.0, extents.z), 1.0);

// 	v[4] = v[0];
// 	v[4].y += extents.y;
// 	v[5] = v[1];
// 	v[5].y += extents.y;
// 	v[6] = v[2];
// 	v[6].y += extents.y;
// 	v[7] = v[3];
// 	v[7].y += extents.y;

//     const float infinity = 1.0 / 0.0;

// 	vec3 newOrigin = vec3(infinity);
// 	vec3 newExtents = vec3(-infinity);
// 	for (uint i = 0; i < 8; ++i)
// 	{
//         vec4 vert = transformMatrix * v[i];

// 		newOrigin = vec3(min(newOrigin.x, vert.x), min(newOrigin.y, vert.y), min(newOrigin.z, vert.z));
// 		newExtents = vec3(max(newExtents.x, vert.x), max(newExtents.y, vert.y), max(newExtents.z, vert.z));
// 	}

// 	origin = newOrigin;
// 	extents = newExtents;
// 	extents -= origin;
// }

bool IsInFrontOfPlane(vec4 plane, in vec3 origin, in vec3 extents)
{
	vec3 centre = origin + (extents * 0.5);
	vec3 halfExtents = extents * 0.5;

	float r = halfExtents.x * abs(plane.x) +
		halfExtents.y * abs(plane.y) + halfExtents.z * abs(plane.z);

	float signedDist = dot(plane.xyz, centre) - plane.w;

	return (-r <= signedDist);
}

float LengthSqr(in vec3 length)
{
	return dot(length, length);
}

float GetTransformLargestScale(in mat4 transformMatrix)
{
	float scaleSqr = max(max(LengthSqr(transformMatrix[0].xyz), LengthSqr(transformMatrix[1].xyz)), LengthSqr(transformMatrix[2].xyz));
	return sqrt(scaleSqr);
}

void TransformSphere(in mat4 transformMatrix, inout vec3 center, inout float radius)
{
	center = (transformMatrix * vec4(center, 1.0)).xyz;
	radius = radius * GetTransformLargestScale(transformMatrix);
}

bool IsInFrontOfPlane(vec4 plane, in vec3 center, float radius)
{
	float signedDist = dot(plane.xyz, center) - plane.w;
	return signedDist + radius > 0;
}
