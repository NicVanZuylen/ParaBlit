#include "Mesh.h"
#include <vector>
#include <iostream>

// Using tiny obj loader header lib for .obj file loading.
#define TINYOBJLOADER_IMPLEMENTATION
#include "TinyObjLoader/include/tiny_obj_loader.h"

namespace PBClient
{
	Mesh::Mesh(PB::IRenderer* renderer, const char* filePath)
	{
		m_renderer = renderer;
		m_empty = true;
		m_filePath = filePath;

		// Use the filename as the name.
		std::string tmpName = filePath;
		m_name = "|" + tmpName.substr(tmpName.find_last_of('/') + 1) + "|";

		Load(filePath);
	}

	Mesh::~Mesh()
	{
		if (!m_empty)
		{
			m_empty = true;

			m_renderer->FreeBuffer(m_vertexBuffer);
			m_vertexBuffer = nullptr;
			m_renderer->FreeBuffer(m_indexBuffer);
			m_indexBuffer = nullptr;
		}
	}

	void Mesh::Load(const char* filePath)
	{
		// Delete old vertex buffer if there is one.
		if (!m_empty)
		{
			m_empty = true;

			m_renderer->FreeBuffer(m_vertexBuffer);
			m_vertexBuffer = nullptr;
			m_renderer->FreeBuffer(m_indexBuffer);
			m_indexBuffer = nullptr;
		}

		m_filePath = filePath;

		// Cache reading

		std::string cachePath = m_filePath;
		size_t nExtensionStart = cachePath.find_last_of(".");

		cachePath.erase(nExtensionStart); // Remove old file extension.
		cachePath.append(".mcache"); // Append new file extension.

		std::ifstream cacheInStream(cachePath.c_str(), std::ios::binary | std::ios::in);

		// Read cache if one is found.
		if (cacheInStream.good())
		{
			std::cout << "Mesh: Reading cache file at: " << cachePath << "\n";

			MeshCacheData inCacheData;

			// Read cache data...
			cacheInStream.read((char*)&inCacheData, sizeof(MeshCacheData));

			// Find size of the vertex and index buffers.
			uint64_t vertexBufferSize = inCacheData.m_vertexCount * sizeof(Vertex);
			uint64_t indexBufferSize = inCacheData.m_indexCount * sizeof(MeshIndex);

			m_totalVertexCount = inCacheData.m_vertexCount;
			m_totalIndexCount = inCacheData.m_indexCount;

			PB::BufferObjectDesc vertexBufferDesc;
			vertexBufferDesc.m_bufferSize = vertexBufferSize;
			vertexBufferDesc.m_options = 0;
			vertexBufferDesc.m_usage = PB::PB_BUFFER_USAGE_COPY_DST | PB::PB_BUFFER_USAGE_VERTEX;
			m_vertexBuffer = m_renderer->AllocateBuffer(vertexBufferDesc);

			PB::BufferObjectDesc indexBufferDesc;
			indexBufferDesc.m_bufferSize = indexBufferSize;
			indexBufferDesc.m_options = 0;
			indexBufferDesc.m_usage = PB::PB_BUFFER_USAGE_COPY_DST | PB::PB_BUFFER_USAGE_INDEX;
			m_indexBuffer = m_renderer->AllocateBuffer(indexBufferDesc);

			// Move read offset to vertex start offset.
			cacheInStream.seekg(inCacheData.m_vertexDataOffset);

			// Read vertex data...
			char* vertexAddress = reinterpret_cast<char*>(m_vertexBuffer->BeginPopulate());
			cacheInStream.read(vertexAddress, inCacheData.m_vertexCount * sizeof(Vertex));
			m_vertexBuffer->EndPopulate();

			// Move read offset to index start offset.
			cacheInStream.seekg(inCacheData.m_indexOffset);

			// Read index data...
			char* indexAddress = reinterpret_cast<char*>(m_indexBuffer->BeginPopulate());
			cacheInStream.read(indexAddress, inCacheData.m_indexCount * sizeof(MeshIndex));
			m_indexBuffer->EndPopulate();

			// Release file handle.
			cacheInStream.close();
		}
		else // Otherwise load and convert OBJ file.
		{
			// Array of all vertices of all mesh chunks, for a single mesh VBO.
			CLib::Vector<Vertex> wholeMeshVertices;
			CLib::Vector<MeshIndex> wholeMeshIndices;

			LoadOBJ(wholeMeshVertices, wholeMeshIndices, m_filePath);

			uint64_t vertexBufferSize = wholeMeshVertices.Count() * sizeof(Vertex);
			uint64_t indexBufferSize = wholeMeshIndices.Count() * sizeof(MeshIndex);

			m_totalVertexCount = wholeMeshVertices.Count();
			m_totalIndexCount = wholeMeshIndices.Count();

			// -----------------------------------------------------------------------------------------
			// Cache writing

			std::ofstream cacheOutStream(cachePath.c_str(), std::ios::binary | std::ios::out);

			MeshCacheData outCacheData;
			outCacheData.m_vertexCount = wholeMeshVertices.Count();
			outCacheData.m_indexCount = wholeMeshIndices.Count();
			outCacheData.m_vertexDataOffset = sizeof(MeshCacheData);
			outCacheData.m_indexOffset = outCacheData.m_vertexDataOffset + vertexBufferSize;

			if (cacheOutStream.good())
			{
				std::cout << "Mesh: Writing cache file at: " << cachePath << "\n";

				// Write cache data.
				cacheOutStream.write((const char*)&outCacheData, sizeof(MeshCacheData));

				// Seek to vertex data start offset.
				cacheOutStream.seekp(outCacheData.m_vertexDataOffset);

				// Write vertex data...
				cacheOutStream.write((const char*)wholeMeshVertices.Data(), vertexBufferSize);

				// Seek to index data start offset.
				cacheOutStream.seekp(outCacheData.m_indexOffset);

				// Write index data.
				cacheOutStream.write((const char*)wholeMeshIndices.Data(), indexBufferSize);

				// Release file handle.
				cacheOutStream.close();
			}

			// -----------------------------------------------------------------------------------------

			// Create vertex and index buffers...
			PB::BufferObjectDesc vertexBufferDesc;
			vertexBufferDesc.m_bufferSize = vertexBufferSize;
			vertexBufferDesc.m_options = 0;
			vertexBufferDesc.m_usage = PB::PB_BUFFER_USAGE_COPY_DST | PB::PB_BUFFER_USAGE_VERTEX;
			m_vertexBuffer = m_renderer->AllocateBuffer(vertexBufferDesc);

			PB::BufferObjectDesc indexBufferDesc;
			indexBufferDesc.m_bufferSize = indexBufferSize;
			indexBufferDesc.m_options = 0;
			indexBufferDesc.m_usage = PB::PB_BUFFER_USAGE_COPY_DST | PB::PB_BUFFER_USAGE_INDEX;
			m_indexBuffer = m_renderer->AllocateBuffer(indexBufferDesc);

			// Copy vertex data...
			char* vertexAddress = reinterpret_cast<char*>(m_vertexBuffer->BeginPopulate());
			memcpy(vertexAddress, wholeMeshVertices.Data(), vertexBufferSize);
			m_vertexBuffer->EndPopulate();

			// Copy index data...
			char* indexAddress = reinterpret_cast<char*>(m_indexBuffer->BeginPopulate());
			memcpy(indexAddress, wholeMeshIndices.Data(), indexBufferSize);
			m_indexBuffer->EndPopulate();
		}

		m_empty = false;
	}

	uint32_t Mesh::VertexCount()
	{
		return m_totalVertexCount;
	}

	uint32_t Mesh::IndexCount()
	{
		return m_totalIndexCount;
	}

	const std::string& Mesh::GetName()
	{
		return m_name;
	}

	const PB::IBufferObject* Mesh::GetVertexBuffer()
	{
		return m_vertexBuffer;
	}

	const PB::IBufferObject* Mesh::GetIndexBuffer()
	{
		return m_indexBuffer;
	}

	void Mesh::CalculateTangents(CLib::Vector<Vertex>& vertices, CLib::Vector<MeshIndex>& indices)
	{
		uint32_t nIndexCount = indices.Capacity();

		for (uint32_t i = 0; i < nIndexCount; i += 3) // Step 3 indices, as this should run once per triangle
		{
			// Tangents must be calculated per triangle. We find the correct vertices per tri by using the indices.

			// Positions
			glm::vec3 pos1 = vertices[indices[i]].m_position;
			glm::vec3 pos2 = vertices[indices[i + 1]].m_position;
			glm::vec3 pos3 = vertices[indices[i + 2]].m_position;

			// Tex coords
			glm::vec2 tex1 = vertices[indices[i]].m_texCoords;
			glm::vec2 tex2 = vertices[indices[i + 1]].m_texCoords;
			glm::vec2 tex3 = vertices[indices[i + 2]].m_texCoords;

			// Delta positions
			glm::vec3 dPos1 = pos2 - pos1;
			glm::vec3 dPos2 = pos3 - pos1;

			// Delta tex coords
			glm::vec2 dTex1 = tex2 - tex1;
			glm::vec2 dTex2 = tex3 - tex1;

			float f = 1.0f / (dTex1.x * dTex2.y - dTex2.x * dTex1.y);

			// Calculate tangent only. The bitangent will be calculated in the vertex shader.
			glm::vec4 tangent;
			tangent.x = f * (dTex2.y * dPos1.x - dTex1.y * dPos2.x);
			tangent.y = f * (dTex2.y * dPos1.y - dTex1.y * dPos2.y);
			tangent.z = f * (dTex2.y * dPos1.z - dTex1.y * dPos2.z);
			tangent = glm::normalize(tangent);

			// Assign tangents to vertices.
			vertices[indices[i]].m_tangent = tangent;
			vertices[indices[i + 1]].m_tangent = tangent;
			vertices[indices[i + 2]].m_tangent = tangent;
		}
	}

	void Mesh::LoadOBJ(CLib::Vector<Vertex>& vertices, CLib::Vector<MeshIndex>& indices, const char* path)
	{
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string errorMessage;

		// Load meshes and materials from the OBJ file.
		bool loadSuccess = tinyobj::LoadObj(shapes, materials, errorMessage, path, nullptr);

		if (!loadSuccess)
		{
			std::cout << "Mesh Error: Error loading OBJ: " + errorMessage << std::endl;
			return;
		}

		uint32_t chunkCount = static_cast<uint32_t>(shapes.size());

		size_t totalVertexCount = 0;
		size_t totalIndexCount = 0;

		// Determine size of whole mesh and allocate memory.
		for (int i = 0; i < chunkCount; ++i)
		{
			totalVertexCount += shapes[i].mesh.positions.size();
			totalIndexCount += shapes[i].mesh.indices.size();
		}

		totalVertexCount /= 3; // Divide by 3 since positions are floats rather than vec3s.

		vertices.Reserve(static_cast<uint32_t>(totalVertexCount));
		indices.Reserve(static_cast<uint32_t>(totalIndexCount));

		for (uint32_t i = 0; i < chunkCount; ++i)
		{
			tinyobj::shape_t& shape = shapes[i];

			CLib::Vector<MeshIndex> chunkIndices(static_cast<uint32_t>(shape.mesh.indices.size()));
			chunkIndices.SetCount(chunkIndices.Capacity());
			memcpy_s(chunkIndices.Data(), sizeof(MeshIndex) * chunkIndices.Capacity(), shape.mesh.indices.data(), sizeof(MeshIndex) * shape.mesh.indices.size());

			// Append chunk indices to whole mesh indices...
			if (i == 0) // The indices can be copied for the first submesh, since they do not need to be offset by vertex count.
			{
				indices.Reserve(indices.Count() + static_cast<uint32_t>(shape.mesh.indices.size()));

				int chunkIndicesSize = sizeof(MeshIndex) * static_cast<uint32_t>(shape.mesh.indices.size());
				memcpy_s(&indices.Data()[indices.Count()], chunkIndicesSize, shape.mesh.indices.data(), chunkIndicesSize);

				indices.SetCount(indices.Capacity());
			}
			else // Other wise they need to be pushed one by one with the vertex count offset.
			{
				for (int i = 0; i < shape.mesh.indices.size(); ++i)
					indices.PushBack(shape.mesh.indices[i] + vertices.Count());
			}

			// Set up chunk vertices and add to whole mesh vertices.
			CLib::Vector<Vertex> chunkVertices;
			chunkVertices.Reserve(static_cast<uint32_t>(shape.mesh.positions.size() / 3)); // Divide size by 3 to account for the fact that the array is of floats rather than vector structs.
			chunkVertices.SetCount(chunkVertices.Capacity());

			for (uint32_t j = 0; j < chunkVertices.Count(); ++j)
			{
				// Positions, normals etc are stored in float format, in groups. (3 for positions and normals, 2 for tex coords).
				// Multiply the index to stride to the current float group.
				uint32_t nIndex = j * 3; // Stride of three floats for positions and normals.
				uint32_t nTexIndex = j * 2; // Stride of two floats for texture coordinates.

				// Copy positions...
				if (shape.mesh.positions.size())
					chunkVertices[j].m_position = glm::vec4(shape.mesh.positions[nIndex], shape.mesh.positions[nIndex + 1], shape.mesh.positions[nIndex + 2], 1.0f);
				else
					chunkVertices[j].m_position = glm::vec4(0.0f);

				// Copy normals...
				if (shape.mesh.normals.size())
					chunkVertices[j].m_normal = glm::vec4(shape.mesh.normals[nIndex], shape.mesh.normals[nIndex + 1], shape.mesh.normals[nIndex + 2], 1.0f);
				else
					chunkVertices[j].m_normal = glm::vec4(0.0f);

				// Copy texture coordinates.
				if (shape.mesh.texcoords.size())
					chunkVertices[j].m_texCoords = glm::vec2(shape.mesh.texcoords[nTexIndex], 1.0f - shape.mesh.texcoords[nTexIndex + 1]);
				else
					chunkVertices[j].m_texCoords = glm::vec2(0.0f);

				// Add the vertex to the whole mesh vertex array.
				vertices.PushBack(chunkVertices[j]);
			}

			// Calculate tangents for each vertex...
			CalculateTangents(chunkVertices, chunkIndices);
		}

		// Calculate tangents for whole mesh...
		CalculateTangents(vertices, indices);
	}
}