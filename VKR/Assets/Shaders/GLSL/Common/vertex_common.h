struct VS_IN
{
    vec3 position;
    uint texCoordPacked;
    uint normalPackedA;
    uint normalPackedB;
    uint tangentPackedA;
    uint tangentPackedB;
};

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