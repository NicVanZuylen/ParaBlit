#include "MeshEncoder.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <chrono>
#include <cassert>

namespace AssetPipeline
{
	MeshEncoder::MeshEncoder(const char* name, const char* dbName, const char* assetDirectory)
		: EncoderBase(name, dbName, assetDirectory)
	{
		std::vector<AssetEncoder::FileInfo> fileInfos;
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".obj");

		std::vector<AssetStatus> assetStatus;
		GetAssetStatus("Meshes", fileInfos, assetStatus);

		for (auto& asset : assetStatus)
		{
			if (asset.m_buildRequired)
			{
				BuildMesh(asset);
			}
		}
	}

	MeshEncoder::~MeshEncoder()
	{

	}

	void MeshEncoder::CalculateTangents(VertexBuffer& vertices, IndexBuffer& indices)
	{
		uint32_t nIndexCount = indices.Count();

		for (uint32_t i = 0; i < nIndexCount; i += 3) // Step 3 indices, as this should run once per triangle
		{
			// Tangents must be calculated per triangle. We find the correct vertices per tri by using the indices.

			// Positions
			glm::vec4 pbpos1 = vertices[indices[i]].m_position;
			glm::vec4 pbpos2 = vertices[indices[i + 1]].m_position;
			glm::vec4 pbpos3 = vertices[indices[i + 2]].m_position;

			// Tex coords
			glm::vec2 pbtex1 = vertices[indices[i]].m_texCoords;
			glm::vec2 pbtex2 = vertices[indices[i + 1]].m_texCoords;
			glm::vec2 pbtex3 = vertices[indices[i + 2]].m_texCoords;

			// Positions
			glm::vec3 pos1 = glm::vec3(pbpos1.x, pbpos1.y, pbpos1.z);
			glm::vec3 pos2 = glm::vec3(pbpos2.x, pbpos2.y, pbpos2.z);
			glm::vec3 pos3 = glm::vec3(pbpos3.x, pbpos3.y, pbpos3.z);

			// Tex coords
			glm::vec2 tex1 = glm::vec2(pbtex1.x, pbtex1.y);
			glm::vec2 tex2 = glm::vec2(pbtex2.x, pbtex2.y);
			glm::vec2 tex3 = glm::vec2(pbtex3.x, pbtex3.y);

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
			vertices[indices[i]].m_tangent = glm::vec4(tangent.x, tangent.y, tangent.z, tangent.w);
			vertices[indices[i + 1]].m_tangent = glm::vec4(tangent.x, tangent.y, tangent.z, tangent.w);
			vertices[indices[i + 2]].m_tangent = glm::vec4(tangent.x, tangent.y, tangent.z, tangent.w);
		}
	}

	void MeshEncoder::LoadOBJ(VertexBuffer& vertices, IndexBuffer& indices, const char* path)
	{
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string errorMessage;

		// Load meshes and materials from the OBJ file.
		bool loadSuccess = tinyobj::LoadObj(shapes, materials, errorMessage, path, nullptr);

		if (loadSuccess)
		{
			printf("%s: Successfully loaded OBJ: %s\n", m_name.c_str(), path);
		}
		else
		{
			printf("%s: Error loading OBJ: %s\n", m_name.c_str(), errorMessage.c_str());
			return;
		}

		uint32_t chunkCount = static_cast<uint32_t>(shapes.size());

		size_t totalVertexCount = 0;
		size_t totalIndexCount = 0;

		// Determine size of whole mesh and allocate memory.
		for (uint32_t i = 0; i < chunkCount; ++i)
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

			IndexBuffer chunkIndices(static_cast<uint32_t>(shape.mesh.indices.size()));
			chunkIndices.SetCount(chunkIndices.Capacity());
			memcpy_s(chunkIndices.Data(), sizeof(MeshIndex) * chunkIndices.Count(), shape.mesh.indices.data(), sizeof(MeshIndex) * shape.mesh.indices.size());

			// Append chunk indices to whole mesh indices...
			if (i == 0) // The indices can be copied for the first submesh, since they do not need to be offset by vertex count.
			{
				indices.Reserve(indices.Count() + static_cast<uint32_t>(shape.mesh.indices.size()));

				int chunkIndicesSize = sizeof(MeshIndex) * static_cast<uint32_t>(shape.mesh.indices.size());
				memcpy_s(&indices.Data()[indices.Count()], chunkIndicesSize, shape.mesh.indices.data(), chunkIndicesSize);

				indices.SetCount(indices.Count() + static_cast<uint32_t>(shape.mesh.indices.size()));
			}
			else // Other wise they need to be pushed one by one with the vertex count offset.
			{
				for (int i = 0; i < shape.mesh.indices.size(); ++i)
					indices.PushBack(shape.mesh.indices[i] + vertices.Count());
			}

			// Set up chunk vertices and add to whole mesh vertices.
			VertexBuffer chunkVertices;
			chunkVertices.SetCount(static_cast<uint32_t>(shape.mesh.positions.size() / 3)); // Divide size by 3 to account for the fact that the array is of floats rather than vector structs.

			for (uint32_t j = 0; j < chunkVertices.Count(); ++j)
			{
				// Positions, normals etc are stored in float format, in groups. (3 for positions and normals, 2 for tex coords).
				// Multiply the index to stride to the current float group.
				uint64_t nIndex = static_cast<uint64_t>(j) * 3; // Stride of three floats for positions and normals.
				uint64_t nTexIndex = static_cast<uint64_t>(j) * 2; // Stride of two floats for texture coordinates.

				// Copy positions...
				if (shape.mesh.positions.size())
					chunkVertices[j].m_position = glm::vec4(shape.mesh.positions[nIndex], shape.mesh.positions[nIndex + 1], shape.mesh.positions[nIndex + 2], 1.0f);
				else
					chunkVertices[j].m_position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

				// Copy normals...
				if (shape.mesh.normals.size())
					chunkVertices[j].m_normal = glm::vec4(shape.mesh.normals[nIndex], shape.mesh.normals[nIndex + 1], shape.mesh.normals[nIndex + 2], 1.0f);
				else
					chunkVertices[j].m_normal = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

				// Copy texture coordinates.
				if (shape.mesh.texcoords.size())
					chunkVertices[j].m_texCoords = glm::vec2(shape.mesh.texcoords[nTexIndex], 1.0f - shape.mesh.texcoords[nTexIndex + 1]);
				else
					chunkVertices[j].m_texCoords = glm::vec2(0.0f, 0.0f);

				chunkVertices[j].m_pad0 = glm::vec2(0.0f, 0.0f);

				// Add the vertex to the whole mesh vertex array.
				vertices.PushBack(chunkVertices[j]);
			}

			// Calculate tangents for each vertex...
			CalculateTangents(chunkVertices, chunkIndices);
		}

		// Calculate tangents for whole mesh...
		CalculateTangents(vertices, indices);
	}

	inline void MeshEncoder::BuildMesh(const AssetStatus& asset)
	{
		// Array of all vertices of all mesh chunks, for a single mesh VBO.
		VertexBuffer wholeMeshVertices;
		IndexBuffer wholeMeshIndices;

		LoadOBJ(wholeMeshVertices, wholeMeshIndices, asset.m_fullPath.c_str());

		uint64_t vertexBufferSize = wholeMeshVertices.Count() * sizeof(Vertex);
		uint64_t indexBufferSize = wholeMeshIndices.Count() * sizeof(MeshIndex);

		uint32_t totalVertexCount = wholeMeshVertices.Count();
		uint32_t totalIndexCount = wholeMeshIndices.Count();

		MeshCacheData outCacheData;
		outCacheData.m_vertexCount = wholeMeshVertices.Count();
		outCacheData.m_indexCount = wholeMeshIndices.Count();
		outCacheData.m_vertexDataOffset = sizeof(MeshCacheData);
		outCacheData.m_indexOffset = outCacheData.m_vertexDataOffset + vertexBufferSize;

		size_t totalSize = sizeof(MeshCacheData) + vertexBufferSize + indexBufferSize;
		uint8_t* data = reinterpret_cast<uint8_t*>(m_dbWriter->AllocateAsset(asset.m_dbPath.c_str(), totalSize, asset.m_info.m_dateModified));

		memcpy(data, &outCacheData, sizeof(MeshCacheData));
		memcpy(&data[outCacheData.m_vertexDataOffset], wholeMeshVertices.Data(), vertexBufferSize);
		memcpy(&data[outCacheData.m_indexOffset], wholeMeshIndices.Data(), indexBufferSize);

		printf("%s: Stored encoded mesh %s in database: %s at location: %s\n", m_name.c_str(), asset.m_fullPath.c_str(), m_dbName.c_str(), asset.m_dbPath.c_str());
	}
}