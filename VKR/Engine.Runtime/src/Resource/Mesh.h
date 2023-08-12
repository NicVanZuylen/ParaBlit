#ifndef MESH_H
#define MESH_H

#include "CLib/Vector.h"
#include <string>

#include "Engine.ParaBlit/IRenderer.h"
#include "WorldRender/DrawBatch.h"
#include "Engine.AssetEncoder/AssetDatabaseReader.h"

namespace Eng
{
	struct Vertex
	{
		PB::Float3 m_position;
		uint32_t m_texCoordsPacked;
		uint32_t m_normalPackedA;
		uint32_t m_normalPackedB;
		uint32_t m_tangentPackedA;
		uint32_t m_tangentPackedB;
	};
	static_assert(sizeof(Vertex) == 32);

	struct Meshlet
	{
		glm::vec3 m_origin;
		uint32_t m_vertexOffset;
		glm::vec3 m_extents;
		uint32_t m_primitiveOffset;
		uint32_t m_vertexCount;
		uint32_t m_primitiveCount;
		uint32_t m_normalDataXYPacked;
		uint32_t m_normalDataZThetaPacked;
	};
	static_assert(sizeof(Meshlet) == 12 * sizeof(uint32_t));

	struct MeshCacheData
	{
		uint64_t m_vertexCount = 0;
		uint64_t m_indexCount = 0;
		uint64_t m_meshletCount = 0;
		uint64_t m_meshletPrimitiveCount = 0;
		size_t m_vertexDataOffset = 0;
		size_t m_indexOffset = 0;
		size_t m_meshletDataOffset = 0;
		size_t m_meshletPrimitiveDataOffset = 0;
		glm::vec4 m_boundOrigin;
		glm::vec4 m_boundExtents;
	};

	typedef uint32_t MeshIndex;

	class Mesh
	{
	public:

		static AssetEncoder::AssetBinaryDatabaseReader s_meshDatabaseLoader;

		Mesh() = default;

		Mesh(AssetEncoder::AssetID assetID, AssetEncoder::AssetBinaryDatabaseReader* databaseReader = &s_meshDatabaseLoader);

		~Mesh();

		/*
		Description: Constructor logic.
		*/
		void Init(AssetEncoder::AssetID assetID, AssetEncoder::AssetBinaryDatabaseReader* databaseReader = &s_meshDatabaseLoader);

		/*
		Description: Load the mesh from a file, and any included materials.
		Param:
			const char* filePath: The path to the .obj mesh file.
			bool loadFromDatabase: Whether or not to load the mesh from an asset database (.adb) file, and treat filePath as a database-local path.
		*/
		void Load(PB::IRenderer* renderer, AssetEncoder::AssetBinaryDatabaseReader* databaseReader = &s_meshDatabaseLoader);

		/*
		Description: Get the asset ID used for loading the mesh.
		Return Type: AssetEncoder::AssetID
		*/
		AssetEncoder::AssetID GetAssetID() const { return m_assetID; }

		/*
		Description: Get the amount of vertices in the entire mesh.
		Return Type: uint32_t
		*/
		uint32_t VertexCount() const;

		/*
		Description: Get the amount of indices in the entire mesh.
		Return Type: uint32_t
		*/
		uint32_t IndexCount() const;

		/*
		Description: Get the amount of meshlet primitives in the entire mesh.
		Return Type: uint32_t
		*/
		uint32_t MeshletPrimitiveCount() const;

		/*
		Description: Get index of the first vertex in the vertex pool (if vertex pool is used).
		Return Type: uint32_t
		*/
		uint32_t FirstVertex() const;

		/*
		Description: Get the mesh vertex buffer.
		Return Type: const PB::IBufferObject*
		*/
		PB::IBufferObject* GetVertexBuffer();

		/*
		Description: Get the mesh index buffer.
		Return Type: const PB::IBufferObject*
		*/
		PB::IBufferObject* GetIndexBuffer();

		/*
		Description: Get the meshlet buffer of this mesh.
		Return Type: const PB::IBufferObject*
		*/
		PB::IBufferObject* GetMeshletBuffer();

		/*
		Description: Get the mesh vertex buffer.
		Return Type: const PB::IBufferObject*
		*/
		const PB::IBufferObject* GetVertexBuffer() const;

		/*
		Description: Get the mesh index buffer.
		Return Type: const PB::IBufferObject*
		*/
		const PB::IBufferObject* GetIndexBuffer() const;

		/*
		Description: Get the meshlet buffer of this mesh.
		Return Type: const PB::IBufferObject*
		*/
		const PB::IBufferObject* GetMeshletBuffer() const;

		/*
		Description: Get the primitive buffer of this mesh.
		Return Type: const PB::IBufferObject*
		*/
		const PB::IBufferObject* GetMeshletPrimitiveBuffer() const;

		/*
		Description: Get the axis-aligned bounding box which fully encapsulates the mesh at identity transform.
		Return Type: Bounds
		*/
		Bounds GetBounds() const { return m_bounds; }

		/*
		Description: Get the vertex pool this mesh was allocated from.
		Return Type: const VertexPool*
		*/
		const VertexPool* GetVertexPool() const;

		static void GetMeshData(AssetEncoder::AssetID assetID, MeshCacheData* outData);

	private:

		AssetEncoder::AssetID m_assetID = 0;
		PB::IRenderer* m_renderer = nullptr;
		PB::IBufferObject* m_vertexBuffer = nullptr;
		PB::IBufferObject* m_indexBuffer = nullptr;
		PB::IBufferObject* m_meshletBuffer = nullptr;
		PB::IBufferObject* m_meshletPrimitiveBuffer = nullptr;
		uint64_t m_totalVertexCount = 0;
		uint64_t m_totalIndexCount = 0;
		uint64_t m_totalMeshletPrimitiveCount = 0;
		Bounds m_bounds;
		const char* m_filePath = nullptr;
		VertexPool* m_vertexPool = nullptr;
		uint64_t m_firstVertexInPool = 0;
		bool m_empty = true;
	};
}

#endif /* MESH_H */