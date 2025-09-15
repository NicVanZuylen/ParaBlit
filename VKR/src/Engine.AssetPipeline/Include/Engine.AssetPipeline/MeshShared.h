#pragma once
#include "Engine.Math/Vector4.h"

namespace AssetPipeline
{
	typedef uint32_t MeshIndex;

	static constexpr uint32_t DefaultLODCount = 4;
	static constexpr uint32_t MaxLODCount = 8;
	static constexpr uint32_t LODComplexityReductionFactor = 8;
	static constexpr uint32_t VertexOffsetBits = 26;
	static constexpr uint32_t PrimitiveOffsetBits = 26;

	struct Vertex
	{
		Eng::Math::Vector3f m_position;
		uint32_t m_texCoords;
		uint64_t m_normal;
		uint64_t m_tangent;
	};
	static_assert(sizeof(Vertex) == 32);

	struct Meshlet
	{
		Eng::Math::Vector3f m_center;
		float m_radius;
		uint32_t m_normalDataXYPacked;
		uint32_t m_normalDataZThetaPacked;
		uint32_t m_vertOffsetCountPacked;
		uint32_t m_primOffsetCountPacked;
	};
	static_assert(sizeof(Meshlet) == 8 * sizeof(uint32_t));

	struct MeshCacheData
	{
		uint64_t m_vertexCount;
		uint64_t m_indexCount;
		uint64_t m_meshletCount;
		uint64_t m_meshletPrimitiveCount;
		uint64_t m_blasIndexCount;
		size_t m_vertexDataOffset;
		size_t m_indexOffset;
		size_t m_meshletDataOffset;
		size_t m_meshletPrimitiveDataOffset;
		size_t m_blasIndexBufferDataOffset;
		Eng::Math::Vector4f m_boundOrigin;
		Eng::Math::Vector4f m_boundExtents;
		uint32_t m_lodMeshletEndOffsets[MaxLODCount]; // Offsets of end of the range for each LOD's meshlet data.
		uint32_t m_lodBLASIndexEndOffsets[MaxLODCount]; // Offsets of end of the range for each LOD's BLAS index data.
		uint8_t m_lodCount;
		uint8_t m_lodMask;
		uint8_t m_maxLOD;
		uint8_t m_pad;
	};
}