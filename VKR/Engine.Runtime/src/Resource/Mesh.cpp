#include "Mesh.h"
#include "DrawBatch.h"

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
			m_empty = true;

			if(m_vertexBuffer)
				m_renderer->FreeBuffer(m_vertexBuffer);
			m_vertexBuffer = nullptr;

			if(m_indexBuffer)
				m_renderer->FreeBuffer(m_indexBuffer);
			m_indexBuffer = nullptr;

			if (m_meshletBuffer)
				m_renderer->FreeBuffer(m_meshletBuffer);
			m_meshletBuffer = nullptr;
		}
	}

	void Mesh::Init(AssetEncoder::AssetID assetID, AssetEncoder::AssetBinaryDatabaseReader* databaseReader)
	{
		m_assetID = assetID;

		const AssetEncoder::AssetMeta& meta = databaseReader->GetAssetInfo(m_assetID);
		MeshCacheData data;
		databaseReader->GetAssetUserData(meta, &data);

		m_totalVertexCount = data.m_vertexCount;
		m_totalIndexCount = data.m_indexCount;
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
		MeshCacheData cacheData;
		s_meshDatabaseLoader.GetAssetUserData(assetInfo, &cacheData);

		size_t vertexBufferSize = cacheData.m_vertexCount * sizeof(Vertex);
		size_t indexBufferSize = cacheData.m_indexCount * sizeof(MeshIndex);
		size_t meshletBufferSize = cacheData.m_meshletCount * sizeof(Meshlet);

		// Create vertex and index buffers...
		PB::BufferObjectDesc vertexBufferDesc;
		vertexBufferDesc.m_bufferSize = static_cast<PB::u32>(vertexBufferSize);
		vertexBufferDesc.m_options = 0;
		vertexBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::STORAGE;

		if (m_vertexPool == nullptr)
		{
			// Allocate a standalone buffer for the vertices.
			m_vertexBuffer = m_renderer->AllocateBuffer(vertexBufferDesc);

			PB::u8* vertexData = m_vertexBuffer->BeginPopulate();
			s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, vertexData, cacheData.m_vertexDataOffset, cacheData.m_vertexDataOffset + vertexBufferSize);
			m_vertexBuffer->EndPopulate();
		}
		else
		{
			// Allocate a placed buffer for vertex pools.
			vertexBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::STORAGE;
			vertexBufferDesc.m_options = PB::EBufferOptions::POOL_PLACED;
			m_vertexBuffer = m_renderer->AllocateBuffer(vertexBufferDesc);

			PB::u32 placementSizeBytes;
			PB::u32 placementAlignBytes;
			m_vertexBuffer->GetPlacedResourceSizeAndAlign(placementSizeBytes, placementAlignBytes);

			PB::u32 firstVertex;
			m_vertexPool->GetNextVertexOffset(placementSizeBytes, firstVertex);

			PB::u32 placementLocation = firstVertex * sizeof(Vertex);
			PB::u32 locationModAlign = placementLocation % placementAlignBytes;
			if (locationModAlign != 0)
			{
				PB::u32 alignPad = placementAlignBytes - locationModAlign;
				placementLocation += alignPad;
			}

			m_vertexPool->GetPool()->PlaceBuffer(m_vertexBuffer, placementLocation);
			char* vertexAddress = reinterpret_cast<char*>(m_vertexBuffer->BeginPopulate());

			s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, vertexAddress, cacheData.m_vertexDataOffset, cacheData.m_vertexDataOffset + vertexBufferSize);

			m_vertexBuffer->EndPopulate();
			m_firstVertexInPool = firstVertex;
		}

		PB::BufferObjectDesc indexBufferDesc;
		indexBufferDesc.m_bufferSize = static_cast<PB::u32>(indexBufferSize);
		indexBufferDesc.m_options = 0;
		indexBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::INDEX | PB::EBufferUsage::STORAGE;
		m_indexBuffer = m_renderer->AllocateBuffer(indexBufferDesc);

		PB::u8* indexData = m_indexBuffer->BeginPopulate();
		s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, indexData, cacheData.m_indexOffset, cacheData.m_indexOffset + indexBufferSize);
		m_indexBuffer->EndPopulate();

		if (meshletBufferSize > 0)
		{
			PB::BufferObjectDesc meshletBufferDesc;
			meshletBufferDesc.m_bufferSize = static_cast<PB::u32>(meshletBufferSize);
			meshletBufferDesc.m_options = 0;
			meshletBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::STORAGE;
			m_meshletBuffer = m_renderer->AllocateBuffer(meshletBufferDesc);

			PB::u8* meshletData = m_meshletBuffer->BeginPopulate();
			s_meshDatabaseLoader.GetAssetBinaryRange(m_assetID, meshletData, cacheData.m_meshletDataOffset, cacheData.m_meshletDataOffset + meshletBufferSize);
			m_meshletBuffer->EndPopulate();
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

	const VertexPool* Mesh::GetVertexPool() const
	{
		return m_vertexPool;
	}

	void Mesh::GetMeshData(AssetEncoder::AssetID assetID, MeshCacheData* outData)
	{
		const auto& meta = s_meshDatabaseLoader.GetAssetInfo(assetID);
		s_meshDatabaseLoader.GetAssetUserData(meta, outData);
	}
}