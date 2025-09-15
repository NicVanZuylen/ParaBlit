#include "Mesh.h"
#include "DrawBatch.h"
#include "Engine.Math/Vectors.h"
#include "Shader.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#include "glm/gtc/packing.hpp"
#pragma warning(pop)

#include <vector>
#include <iostream>
#include <filesystem>

// Using tiny obj loader header lib for .obj file loading.
#define TINYOBJLOADER_IMPLEMENTATION
#include "TinyObjLoader/tiny_obj_loader.h"

#ifdef _DEBUG
#define MESH_USE_BUFFER_LABELS 1
#else
#define MESH_USE_BUFFER_LABELS 0
#endif

namespace Eng
{
	AssetEncoder::AssetBinaryDatabaseReader Mesh::s_meshDatabaseLoader;

	Mesh::Mesh(AssetEncoder::AssetID assetID, AssetEncoder::AssetBinaryDatabaseReader* databaseReader)
	{
		Init(assetID);
	}

	Mesh::~Mesh()
	{
		if (!m_empty)
		{
			// Remove from mesh library...
			if(s_meshLibraryBuffer)
			{
				std::lock_guard lock(s_meshLibraryMutex);
				s_meshLibraryBuffer->RemoveInstance(m_libraryInstanceID);
				m_libraryInstanceID = ~ManagedInstanceBuffer::ManagedInstance(0);
			}

			if(m_vertexBuffer)
				m_renderer->FreeBuffer(m_vertexBuffer);
			m_vertexBuffer = nullptr;

			if(m_indexBuffer)
				m_renderer->FreeBuffer(m_indexBuffer);
			m_indexBuffer = nullptr;

			if (m_meshletBuffer)
				m_renderer->FreeBuffer(m_meshletBuffer);
			if (m_meshletPrimitiveBuffer)
				m_renderer->FreeBuffer(m_meshletPrimitiveBuffer);

			m_meshletPrimitiveBuffer = nullptr;
			m_meshletBuffer = nullptr;

			for (uint32_t i = 0; i < m_blas.Count(); ++i)
			{
				m_renderer->FreeAccelerationStructure(m_blas[i]);
			}
			m_blas.Clear();

			if (m_blasIndexDataBuffer)
				m_renderer->FreeBuffer(m_blasIndexDataBuffer);
			m_blasIndexDataBuffer = nullptr;

			m_empty = true;
		}
	}

	void Mesh::Init(AssetEncoder::AssetID assetID, AssetEncoder::AssetBinaryDatabaseReader* databaseReader)
	{
		m_assetID = assetID;

		const AssetEncoder::AssetMeta& meta = databaseReader->GetAssetInfo(m_assetID);
		AssetPipeline::MeshCacheData data;
		databaseReader->GetAssetUserData(meta, &data);

		m_totalVertexCount = data.m_vertexCount;
		m_totalIndexCount = data.m_indexCount;
		m_totalMeshletPrimitiveCount = data.m_meshletPrimitiveCount;
		m_bounds.m_origin = data.m_boundOrigin;
		m_bounds.m_extents = data.m_boundExtents;
	}

	void Mesh::Load(PB::IRenderer* renderer, AssetEncoder::AssetBinaryDatabaseReader* databaseReader)
	{
		m_renderer = renderer;

		// Delete old vertex buffer if there is one.
		if (!m_empty)
		{
			m_empty = true;

			if (m_vertexBuffer)
				m_renderer->FreeBuffer(m_vertexBuffer);
			m_vertexBuffer = nullptr;
			m_renderer->FreeBuffer(m_indexBuffer);
			m_indexBuffer = nullptr;
		}

		m_empty = false;

		constexpr const char* MeshDatabaseDir = "/Assets/build/meshes.adb";
		if (s_meshDatabaseLoader.HasOpenFile() == false)
		{
			std::string dbDir = std::move(std::filesystem::current_path().string());
			dbDir += MeshDatabaseDir;
			s_meshDatabaseLoader.OpenFile(dbDir.c_str());
		}

		const AssetEncoder::AssetMeta& assetInfo = s_meshDatabaseLoader.GetAssetInfo(m_assetID);
		AssetPipeline::MeshCacheData cacheData;
		s_meshDatabaseLoader.GetAssetUserData(assetInfo, &cacheData);

		size_t vertexBufferSize = cacheData.m_vertexCount * sizeof(AssetPipeline::Vertex);
		size_t indexBufferSize = cacheData.m_indexCount * sizeof(MeshIndex);
		size_t meshletBufferSize = cacheData.m_meshletCount * sizeof(AssetPipeline::Meshlet);
		size_t primitiveBufferSize = cacheData.m_meshletPrimitiveCount * sizeof(uint32_t);

		bool hasBLAS = cacheData.m_blasIndexCount > 0;

		std::string bufferName;
		if constexpr (MESH_USE_BUFFER_LABELS)
		{
			bufferName = s_meshDatabaseLoader.GetAssetName(m_assetID);
			bufferName += ":vertex";
		}

		// Create vertex and index buffers...
		PB::BufferObjectDesc vertexBufferDesc;
		vertexBufferDesc.m_name = bool(MESH_USE_BUFFER_LABELS) ? bufferName.c_str() : nullptr;
		vertexBufferDesc.m_bufferSize = static_cast<PB::u32>(vertexBufferSize);
		vertexBufferDesc.m_options = 0;
		vertexBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::STORAGE;
		{
			if (hasBLAS)
			{
				vertexBufferDesc.m_usage |= PB::EBufferUsage::MEMORY_ADDRESS_ACCESS;
			}

			// Allocate a standalone buffer for the vertices.
			m_vertexBuffer = m_renderer->AllocateBuffer(vertexBufferDesc);

			PB::u8* vertexData = m_vertexBuffer->BeginPopulate();
			s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, vertexData, cacheData.m_vertexDataOffset, cacheData.m_vertexDataOffset + vertexBufferSize);
			m_vertexBuffer->EndPopulate();
		}

		if constexpr (MESH_USE_BUFFER_LABELS)
		{
			size_t pos = bufferName.find_last_of(':');
			bufferName.erase(pos, bufferName.size() - pos);

			bufferName += ":index";
		}

		PB::BufferObjectDesc indexBufferDesc;
		indexBufferDesc.m_name = bool(MESH_USE_BUFFER_LABELS) ? bufferName.c_str() : nullptr;
		indexBufferDesc.m_bufferSize = static_cast<PB::u32>(indexBufferSize);
		indexBufferDesc.m_options = 0;
		indexBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::INDEX | PB::EBufferUsage::STORAGE;
		m_indexBuffer = m_renderer->AllocateBuffer(indexBufferDesc);

		PB::u8* indexData = m_indexBuffer->BeginPopulate();
		s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, indexData, cacheData.m_indexOffset, cacheData.m_indexOffset + indexBufferSize);
		m_indexBuffer->EndPopulate();

		if (meshletBufferSize > 0)
		{
			if constexpr (MESH_USE_BUFFER_LABELS)
			{
				size_t pos = bufferName.find_last_of(':');
				bufferName.erase(pos, bufferName.size() - pos);

				bufferName += ":meshlet";
			}

			PB::BufferObjectDesc meshletBufferDesc;
			meshletBufferDesc.m_name = bool(MESH_USE_BUFFER_LABELS) ? bufferName.c_str() : nullptr;
			meshletBufferDesc.m_bufferSize = static_cast<PB::u32>(meshletBufferSize);
			meshletBufferDesc.m_options = 0;
			meshletBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::STORAGE;
			m_meshletBuffer = m_renderer->AllocateBuffer(meshletBufferDesc);

			PB::u8* meshletData = m_meshletBuffer->BeginPopulate();
			s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, meshletData, cacheData.m_meshletDataOffset, cacheData.m_meshletDataOffset + meshletBufferSize);
			m_meshletBuffer->EndPopulate();
		}

		if (primitiveBufferSize > 0)
		{
			if constexpr (MESH_USE_BUFFER_LABELS)
			{
				size_t pos = bufferName.find_last_of(':');
				bufferName.erase(pos, bufferName.size() - pos);

				bufferName += ":primitive";
			}

			PB::BufferObjectDesc primitiveBufferDesc;
			primitiveBufferDesc.m_name = bool(MESH_USE_BUFFER_LABELS) ? bufferName.c_str() : nullptr;
			primitiveBufferDesc.m_bufferSize = static_cast<PB::u32>(primitiveBufferSize);
			primitiveBufferDesc.m_options = 0;
			primitiveBufferDesc.m_usage = PB::EBufferUsage::COPY_SRC | PB::EBufferUsage::COPY_DST | PB::EBufferUsage::STORAGE;
			m_meshletPrimitiveBuffer = m_renderer->AllocateBuffer(primitiveBufferDesc);

			PB::u8* primitiveData = m_meshletPrimitiveBuffer->BeginPopulate();
			s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, primitiveData, cacheData.m_meshletPrimitiveDataOffset, cacheData.m_meshletPrimitiveDataOffset + primitiveBufferSize);
			m_meshletPrimitiveBuffer->EndPopulate();
		}

		if (hasBLAS && m_renderer->GetDeviceLimitations()->m_supportRaytracing)
		{
			size_t blasIndexBufferSize = cacheData.m_blasIndexCount * sizeof(MeshIndex);

			if constexpr (MESH_USE_BUFFER_LABELS)
			{
				size_t pos = bufferName.find_last_of(':');
				bufferName.erase(pos, bufferName.size() - pos);

				bufferName += ":blasIndices";
			}

			PB::BufferObjectDesc blasIndexBufferDesc;
			blasIndexBufferDesc.m_name = bool(MESH_USE_BUFFER_LABELS) ? bufferName.c_str() : nullptr;
			blasIndexBufferDesc.m_bufferSize = static_cast<PB::u32>(blasIndexBufferSize);
			blasIndexBufferDesc.m_options = 0;
			blasIndexBufferDesc.m_usage = PB::EBufferUsage::MEMORY_ADDRESS_ACCESS | PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
			m_blasIndexDataBuffer = m_renderer->AllocateBuffer(blasIndexBufferDesc);

			PB::u8* blasIndexData = m_blasIndexDataBuffer->BeginPopulate();
			s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, blasIndexData, cacheData.m_blasIndexBufferDataOffset, cacheData.m_blasIndexBufferDataOffset + blasIndexBufferSize);
			m_blasIndexDataBuffer->EndPopulate();

			m_blas.Clear();
			for (uint8_t lod = 0; lod < cacheData.m_lodCount; ++lod)
			{
				uint64_t indexDataOffset = 0;
				uint64_t indexCount = 0;

				if (lod > 0)
				{
					indexCount = cacheData.m_lodBLASIndexEndOffsets[lod] - cacheData.m_lodBLASIndexEndOffsets[lod - 1];
					indexDataOffset = cacheData.m_lodBLASIndexEndOffsets[lod] - indexCount;
				}
				else
				{
					indexCount = cacheData.m_lodBLASIndexEndOffsets[lod];
					indexDataOffset = 0;
				}

				indexDataOffset *= sizeof(MeshIndex);

				PB::ASGeometryDesc geoDesc;
				auto& tris = geoDesc.triangles;

				tris.maxVertexCount = cacheData.m_vertexCount;
				tris.vertexStrideBytes = sizeof(AssetPipeline::Vertex);
				tris.useHostVertexData = false;
				tris.useHostIndexData = false;
				tris.maxPrimitiveCount = indexCount / 3;
				tris.vertexDataOffsetBytes = 0;
				tris.indexDataOffsetBytes = indexDataOffset;
				tris.deviceVertexData = m_vertexBuffer;
				tris.deviceIndexData = m_blasIndexDataBuffer;

				PB::AccelerationStructureDesc blasDesc;
				blasDesc.type = PB::AccelerationStructureType::BOTTOM_LEVEL;
				blasDesc.geometryInputs = &geoDesc;
				blasDesc.geometryInputCount = 1;

				m_blas.PushBack(m_renderer->AllocateAccelerationStructure(blasDesc));

				m_blas.Back()->Build();
			}
		}

		// Add mesh library entry...
		if(s_meshLibraryBuffer)
		{
			std::lock_guard lock(s_meshLibraryMutex);

			m_libraryInstanceID = s_meshLibraryBuffer->AddInstance();
			MeshLibraryEntry& entry = *reinterpret_cast<MeshLibraryEntry*>(s_meshLibraryBuffer->GetInstanceData(m_libraryInstanceID));
			entry.meshletBuffer = m_meshletBuffer->GetViewAsStorageBuffer();
			entry.primitiveBuffer = m_meshletPrimitiveBuffer->GetViewAsStorageBuffer();
			entry.vertexBuffer = m_vertexBuffer->GetViewAsStorageBuffer();
			entry.indexBuffer = m_indexBuffer->GetViewAsStorageBuffer();
			entry.blasIndexBuffer = m_blasIndexDataBuffer ? m_blasIndexDataBuffer->GetViewAsStorageBuffer() : 0;
			entry.chosenLOD = 0;
			entry.lodCount = cacheData.m_lodCount;
			entry.lodMask = cacheData.m_lodMask;
			entry.maxLOD = cacheData.m_maxLOD;
			std::memcpy(entry.lodMeshletEndOffsets, cacheData.m_lodMeshletEndOffsets, sizeof(MeshLibraryEntry::lodMeshletEndOffsets));

			for(uint32_t lod = 0; lod < AssetPipeline::MaxLODCount; ++lod)
			{
				uint64_t indexDataOffset = 0;
				uint64_t indexCount = 0;

				if (lod > 0)
				{
					indexCount = cacheData.m_lodBLASIndexEndOffsets[lod] - cacheData.m_lodBLASIndexEndOffsets[lod - 1];
					indexDataOffset = cacheData.m_lodBLASIndexEndOffsets[lod] - indexCount;
				}
				else
				{
					indexDataOffset = 0;
				}
				entry.lodBLASIndexOffsets[lod] = indexDataOffset;

				if (lod < m_blas.Count())
				{
					entry.blasDeviceAddresses[lod] = m_blas[lod]->GetDeviceAddress();
				}
				else
				{
					entry.blasDeviceAddresses[lod] = 0;
				}
			}

			s_meshLibraryNeedsUpdate = true;
		}

		m_empty = false;
		printf("Mesh: Successfully loaded asset [%u] (%u bytes) from database: %s\n", uint32_t(m_assetID), uint32_t(assetInfo.m_binarySize), MeshDatabaseDir);
	}

	uint32_t Mesh::VertexCount() const
	{
		return static_cast<uint32_t>(m_totalVertexCount);
	}

	uint32_t Mesh::IndexCount() const
	{
		return static_cast<uint32_t>(m_totalIndexCount);
	}

	uint32_t Mesh::MeshletPrimitiveCount() const
	{
		return static_cast<uint32_t>(m_totalMeshletPrimitiveCount);
	}

	uint32_t Mesh::FirstVertex() const
	{
		return static_cast<uint32_t>(m_firstVertexInPool);
	}

	PB::IBufferObject* Mesh::GetVertexBuffer()
	{
		return m_vertexBuffer;
	}

	PB::IBufferObject* Mesh::GetIndexBuffer()
	{
		return m_indexBuffer;
	}

	PB::IBufferObject* Mesh::GetMeshletBuffer()
	{
		return m_meshletBuffer;
	}

	PB::IBufferObject* Mesh::GetMeshletPrimitiveBuffer()
	{
		return m_meshletPrimitiveBuffer;
	}

	const PB::IBufferObject* Mesh::GetVertexBuffer() const
	{
		return m_vertexBuffer;
	}

	const PB::IBufferObject* Mesh::GetIndexBuffer() const
	{
		return m_indexBuffer;
	}

	const PB::IBufferObject* Mesh::GetMeshletBuffer() const
	{
		return m_meshletBuffer;
	}

	const PB::IBufferObject* Mesh::GetMeshletPrimitiveBuffer() const
	{
		return m_meshletPrimitiveBuffer;
	}

	void Mesh::GetMeshData(AssetEncoder::AssetID assetID, AssetPipeline::MeshCacheData* outData)
	{
		const auto& meta = s_meshDatabaseLoader.GetAssetInfo(assetID);
		s_meshDatabaseLoader.GetAssetUserData(meta, outData);
	}

	void Mesh::InitializeMeshLibrary(PB::IRenderer* renderer, CLib::Allocator* allocator)
	{
		if (s_meshLibraryBuffer == nullptr)
		{
			std::lock_guard lock(s_meshLibraryMutex);

			AssetEncoder::ShaderPermutationTable permTable{};

			ManagedInstanceBuffer::Desc instanceDesc{};
			instanceDesc.m_elementSize = sizeof(MeshLibraryEntry);
			instanceDesc.m_elementCapacity = MeshLibraryExpandRate;
			instanceDesc.m_usage = PB::EBufferUsage::STORAGE;
			instanceDesc.m_autoSwapStagingOnFlush = true;
			instanceDesc.m_copyAll = true;

			s_meshLibraryBuffer = allocator->Alloc<ManagedInstanceBuffer>();
			s_meshLibraryBuffer->Init(renderer, allocator, instanceDesc);
		}
	}

	void Mesh::DestroyMeshLibrary(CLib::Allocator* allocator)
	{
		std::lock_guard lock(s_meshLibraryMutex);
		if (s_meshLibraryBuffer != nullptr)
		{
			allocator->Free(s_meshLibraryBuffer);
			s_meshLibraryBuffer = nullptr;
		}
	}

	void Mesh::MeshLibraryUpdate()
	{
		std::lock_guard lock(s_meshLibraryMutex);
		if (s_meshLibraryNeedsUpdate)
		{
			s_meshLibraryBuffer->FlushChanges();
			s_meshLibraryNeedsUpdate = false;
		}
	}

	PB::ResourceView Mesh::GetMeshLibraryView()
	{
		std::lock_guard lock(s_meshLibraryMutex);
		return s_meshLibraryBuffer->GetBuffer()->GetViewAsStorageBuffer();
	}
}