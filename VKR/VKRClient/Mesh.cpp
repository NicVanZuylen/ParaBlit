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
#include "TinyObjLoader/include/tiny_obj_loader.h"

namespace PBClient
{
	AssetEncoder::AssetBinaryDatabaseReader Mesh::s_meshDatabaseLoader;

	Mesh::Mesh(PB::IRenderer* renderer, const char* filePath, VertexPool* vertexPool)
	{
		Init(renderer, filePath, vertexPool);
	}

	Mesh::~Mesh()
	{
		if (!m_empty)
		{
			m_empty = true;

			if(m_vertexBuffer)
				m_renderer->FreeBuffer(m_vertexBuffer);
			m_vertexBuffer = nullptr;
			m_renderer->FreeBuffer(m_indexBuffer);
			m_indexBuffer = nullptr;
		}
	}

	void Mesh::Init(PB::IRenderer* renderer, const char* filePath, VertexPool* vertexPool)
	{
		m_renderer = renderer;
		m_filePath = filePath;
		m_vertexPool = vertexPool;

		// Use the filename as the name.
		std::string tmpName = filePath;
		m_name = "|" + tmpName.substr(tmpName.find_last_of('/') + 1) + "|";

		Load(filePath);
	}

	void Mesh::Load(const char* filePath)
	{
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

		m_filePath = filePath;
		m_empty = false;

		constexpr const char* MeshDatabaseDir = "/Assets/build/meshes.adb";
		if (s_meshDatabaseLoader.HasOpenFile() == false)
		{
			std::string dbDir = std::move(std::filesystem::current_path().parent_path().string());
			dbDir += MeshDatabaseDir;
			s_meshDatabaseLoader.OpenFile(dbDir.c_str());
		}

		const AssetEncoder::AssetMeta& assetInfo = s_meshDatabaseLoader.GetAssetInfo(filePath);
		MeshCacheData cacheData;
		s_meshDatabaseLoader.GetAssetBinaryRange(filePath, &cacheData, 0, sizeof(MeshCacheData));

		m_totalVertexCount = cacheData.m_vertexCount;
		m_totalIndexCount = cacheData.m_indexCount;

		size_t vertexBufferSize = cacheData.m_vertexCount * sizeof(Vertex);
		size_t indexBufferSize = cacheData.m_indexCount * sizeof(MeshIndex);

		m_bounds.m_origin = cacheData.m_boundOrigin;
		m_bounds.m_extents = cacheData.m_boundExtents;

		// Create vertex and index buffers...
		PB::BufferObjectDesc vertexBufferDesc;
		vertexBufferDesc.m_bufferSize = static_cast<PB::u32>(vertexBufferSize);
		vertexBufferDesc.m_options = 0;
		vertexBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::VERTEX;

		if (m_vertexPool == nullptr)
		{
			// Allocate a standalone buffer for the vertices.
			m_vertexBuffer = m_renderer->AllocateBuffer(vertexBufferDesc);

			PB::u8* vertexData = m_vertexBuffer->BeginPopulate();
			s_meshDatabaseLoader.GetAssetBinaryRange(filePath, vertexData, cacheData.m_vertexDataOffset, cacheData.m_vertexDataOffset + vertexBufferSize);
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

			s_meshDatabaseLoader.GetAssetBinaryRange(filePath, vertexAddress, cacheData.m_vertexDataOffset, cacheData.m_vertexDataOffset + vertexBufferSize);

			m_vertexBuffer->EndPopulate();
			m_firstVertexInPool = firstVertex;
		}

		PB::BufferObjectDesc indexBufferDesc;
		indexBufferDesc.m_bufferSize = static_cast<PB::u32>(indexBufferSize);
		indexBufferDesc.m_options = 0;
		indexBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::INDEX;
		if (m_vertexPool != nullptr)
			indexBufferDesc.m_usage |= PB::EBufferUsage::STORAGE;
		m_indexBuffer = m_renderer->AllocateBuffer(indexBufferDesc);

		PB::u8* indexData = m_indexBuffer->BeginPopulate();
		s_meshDatabaseLoader.GetAssetBinaryRange(filePath, indexData, cacheData.m_indexOffset, cacheData.m_indexOffset + indexBufferSize);
		m_indexBuffer->EndPopulate();

		m_empty = false;

		printf("Mesh: Successfully loaded asset [%s] (%u bytes) from database: %s\n", filePath, uint32_t(assetInfo.m_binarySize), MeshDatabaseDir);
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

	const std::string& Mesh::GetName() const
	{
		return m_name;
	}

	PB::IBufferObject* Mesh::GetVertexBuffer()
	{
		return m_vertexBuffer;
	}

	PB::IBufferObject* Mesh::GetIndexBuffer()
	{
		return m_indexBuffer;
	}

	const PB::IBufferObject* Mesh::GetVertexBuffer() const
	{
		return m_vertexBuffer;
	}

	const PB::IBufferObject* Mesh::GetIndexBuffer() const
	{
		return m_indexBuffer;
	}

	const VertexPool* Mesh::GetVertexPool() const
	{
		return m_vertexPool;
	}
}