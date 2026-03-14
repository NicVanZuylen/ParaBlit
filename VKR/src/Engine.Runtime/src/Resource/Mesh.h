#ifndef MESH_H
#define MESH_H

#include "CLib/Vector.h"
#include <string>

#include "Engine.ParaBlit/IRenderer.h"
#include "WorldRender/DrawBatch.h"
#include "Engine.AssetEncoder/AssetBinaryDatabaseReader.h"
#include "Engine.AssetPipeline/MeshShared.h"
#include "Engine.Math/Vectors.h"
#include "Utility/ManagedInstanceBuffer.h"

namespace Eng
{
	using namespace Math;

	typedef uint32_t MeshIndex;

	class Mesh
	{
	public:

		static AssetEncoder::AssetBinaryDatabaseReader s_meshDatabaseLoader;

		struct MeshLibraryEntry
		{
			PB::ResourceView meshletBuffer;
			PB::ResourceView primitiveBuffer;
			PB::ResourceView vertexBuffer;
			PB::ResourceView indexBuffer;
			PB::ResourceView blasIndexBuffer;
			PB::u32 pad0[2];
			PB::u8 chosenLOD;
			PB::u8 lodCount;
			PB::u8 maxLOD;
			PB::u8 lodMask;
			PB::u32 lodMeshletEndOffsets[AssetPipeline::MaxLODCount];
			PB::u32 lodBLASIndexOffsets[AssetPipeline::MaxLODCount];
			PB::u64 blasDeviceAddresses[AssetPipeline::MaxLODCount];
		};
		static_assert(sizeof(MeshLibraryEntry) % 16 == 0);

		struct BindingSubresource
		{
			static constexpr uint8_t MeshLibraryEntry = ~uint8_t(0);
			static constexpr uint8_t VertexBuffer = 0;
			static constexpr uint8_t IndexBuffer = 1;
			static constexpr uint8_t MeshletBuffer = 2;
			static constexpr uint8_t All = 3;
		};

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
		Description: Get the primitive buffer of this mesh.
		Return Type: PB::IBufferObject*
		*/
		PB::IBufferObject* GetMeshletPrimitiveBuffer();

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

		uint32_t GetLibraryInstanceID() { return m_libraryInstanceID; }

		static void GetMeshData(AssetEncoder::AssetID assetID, AssetPipeline::MeshCacheData* outData);

		static void InitializeMeshLibrary(PB::IRenderer* renderer, CLib::Allocator* allocator);
		static void DestroyMeshLibrary(CLib::Allocator* allocator);
		static void MeshLibraryUpdate();
		static PB::ResourceView GetMeshLibraryView();

	private:

		static inline ManagedInstanceBuffer* s_meshLibraryBuffer = nullptr;
		static inline std::mutex s_meshLibraryMutex;
		static inline bool s_meshLibraryNeedsUpdate = true;
		static constexpr uint32_t MeshLibraryExpandRate = 128;

		AssetEncoder::AssetID m_assetID = 0;
		PB::IRenderer* m_renderer = nullptr;
		PB::IBufferObject* m_vertexBuffer = nullptr;
		PB::IBufferObject* m_indexBuffer = nullptr;
		PB::IBufferObject* m_meshletBuffer = nullptr;
		PB::IBufferObject* m_meshletPrimitiveBuffer = nullptr;
		PB::IBufferObject* m_blasIndexDataBuffer = nullptr;
		CLib::Vector<PB::IAccelerationStructure*, AssetPipeline::MaxLODCount> m_blas;
		uint64_t m_totalVertexCount = 0;
		uint64_t m_totalIndexCount = 0;
		uint64_t m_totalMeshletPrimitiveCount = 0;
		Bounds m_bounds;
		const char* m_filePath = nullptr;
		uint64_t m_firstVertexInPool = 0;
		ManagedInstanceBuffer::ManagedInstance m_libraryInstanceID = ~ManagedInstanceBuffer::ManagedInstance(0);
		bool m_empty = true;
	};
}

#endif /* MESH_H */