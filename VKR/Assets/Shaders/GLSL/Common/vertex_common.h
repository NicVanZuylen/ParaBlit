#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define MAX_LOD_COUNT 8

struct VS_IN
{
    vec3 position;
    uint texCoordPacked;
    uint normalPackedA;
    uint normalPackedB;
    uint tangentPackedA;
    uint tangentPackedB;
};

struct MeshLibraryEntry
{
    uint meshletBufferIndex;
    uint meshletPrimitiveBufferIndex;
    uint vertexBufferIndex;
    uint indexBufferIndex;
    uint blasIndexBufferIndex;
    uint pad0;
    uint pad1;
    uint lodData; // 0x000000FF - chosen LOD, 0x0000FF00 - LOD count, 0x00FF0000 - max LOD, 0xFF000000 - LOD mask
    uint lodMeshletEndOffsets[8];
    uint lodBLASIndexOffsets[8];
    uint64_t blasDeviceAddresses[8];
};

vec2 UnpackTexCoord(in VS_IN vertex)
{
    return unpackHalf2x16(vertex.texCoordPacked);
}

vec4 UnpackNormal(in VS_IN vertex)
{
    return vec4(unpackHalf2x16(vertex.normalPackedA), unpackHalf2x16(vertex.normalPackedB).x, 1.0);
}

vec4 UnpackTangent(in VS_IN vertex)
{
    return vec4(unpackHalf2x16(vertex.tangentPackedA), unpackHalf2x16(vertex.tangentPackedB).x, 1.0);
}

void UnpackVertexAttibutes(in VS_IN vertex, out vec4 position, out vec2 texCoord, out vec4 normal, out vec4 tangent)
{
    position = vec4(vertex.position, 1.0);
    texCoord = unpackHalf2x16(vertex.texCoordPacked);
    normal = UnpackNormal(vertex);
    tangent = UnpackTangent(vertex);
}

void UnpackVertexAttibutes(in VS_IN vertex, out vec4 position, out vec2 texCoord, out vec3 normal, out vec3 tangent)
{
    position = vec4(vertex.position, 1.0);
    texCoord = unpackHalf2x16(vertex.texCoordPacked);
    normal = UnpackNormal(vertex).xyz;
    tangent = UnpackTangent(vertex).xyz;
}

void UnpackVertexAttibutes(in VS_IN vertex, out vec3 position, out vec2 texCoord, out vec3 normal, out vec3 tangent)
{
    position = vertex.position;
    texCoord = unpackHalf2x16(vertex.texCoordPacked);
    normal = UnpackNormal(vertex).xyz;
    tangent = UnpackTangent(vertex).xyz;
}

uint GetChosenLOD(in uint lodData)
{
    return (lodData & 0x000000FF);
}

uint SetChosenLOD(uint lodData, in uint chosenLOD)
{
    lodData &= 0xFFFFFF00;
    lodData |= (chosenLOD & 0x000000FF);

    return lodData;
}

uint GetLODCount(in uint lodData)
{
    return ((lodData & 0x0000FF00) >> 8);
}

uint GetMaxLOD(in uint lodData)
{
    return ((lodData & 0x00FF0000) >> 16);
}

bool IsSkipLOD(in uint lodData, in uint lod)
{
    uint skipData = ((lodData & 0xFF000000) >> 24);
    return bool((skipData >> lod) & 0x1);
}

float LodDistances[MAX_LOD_COUNT] = float[]
(
    20.0,
    40.0,
    100.0,
    200.0,
    500.0,
    1000.0,
    2000.0,
    5000.0
);

uint GetLODLevel(in vec3 worldPosition, in uint lodData, in vec4 nearPlane)
{
    float dist = dot(nearPlane.xyz, worldPosition) - nearPlane.w;
    uint lod = 0;
    uint lastLOD = lod;
    uint maxLOD = GetMaxLOD(lodData);
    for(uint virtualLOD = 0; virtualLOD < MAX_LOD_COUNT; ++virtualLOD)
    {
        if(dist < LodDistances[virtualLOD])
        {
            break;
        }

        if(IsSkipLOD(lodData, virtualLOD) == false)
        {
            lastLOD = lod;
            ++lod;
        }
    }

    return lastLOD;
}

uint GetMeshletCount(in MeshLibraryEntry mesh, in uint lod)
{
    uint endOffset = mesh.lodMeshletEndOffsets[lod];
    uint startOffset = (lod > 0) ? mesh.lodMeshletEndOffsets[lod - 1] : 0;

    return endOffset - startOffset;
}