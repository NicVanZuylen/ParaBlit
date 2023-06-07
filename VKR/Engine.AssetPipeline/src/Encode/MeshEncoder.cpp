#include "MeshEncoder.h"
#include "../Bounds.h"
#include "../MeshletOctree.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../TinyObjLoader/tiny_obj_loader.h"

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
			Ctrl::IConfigFile* propertyFile = Ctrl::IConfigFile::Create(asset.m_propertyFilePath.c_str(), Ctrl::IConfigFile::EOpenMode::OPEN_READ_WRITE);
			auto* data = propertyFile->GetData();
			if (asset.m_hasPropertyFile == false)
			{
				data->SetBooleanValue("Mesh.ConvertCmToM", false);
				data->SetBooleanValue("Mesh.GenerateMeshlets", true);
				propertyFile->WriteData();
			}

			if (asset.m_outdated)
			{
				BuildMesh(asset, data);
				FlagAsModified();
			}
			else
			{
				WriteUnmodifiedAsset(asset);
			}

			Ctrl::IConfigFile::Destroy(propertyFile);
		}
	}

	MeshEncoder::~MeshEncoder()
	{

	}

	void MeshEncoder::CalculateTangents(VertexBuffer& vertices, IndexBuffer& indices, SinglePrecisionTexCoords& texCoords)
	{
		uint32_t nIndexCount = indices.Count();

		for (uint32_t i = 0; i < nIndexCount; i += 3) // Step 3 indices, as this should run once per triangle
		{
			// Tangents must be calculated per triangle. We find the correct vertices per tri by using the indices.

			// Positions
			glm::vec3 pos1 = vertices[indices[i]].m_position;
			glm::vec3 pos2 = vertices[indices[i + 1]].m_position;
			glm::vec3 pos3 = vertices[indices[i + 2]].m_position;

			// Tex coords
			glm::vec2 tex1 = texCoords[indices[i]];
			glm::vec2 tex2 = texCoords[indices[i + 1]];
			glm::vec2 tex3 = texCoords[indices[i + 2]];

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
			vertices[indices[i]].m_tangent = Vec4PackedHalfFloat(tangent.x, tangent.y, tangent.z, 1.0f);
			vertices[indices[i + 1]].m_tangent = Vec4PackedHalfFloat(tangent.x, tangent.y, tangent.z, 1.0f);
			vertices[indices[i + 2]].m_tangent = Vec4PackedHalfFloat(tangent.x, tangent.y, tangent.z, 1.0f);
		}
	}

	void MeshEncoder::LoadOBJ(VertexBuffer& vertices, IndexBuffer& indices, glm::vec3& outOrigin, glm::vec3& outExtents, const char* path, bool cmToM)
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

		outOrigin = glm::vec3(INFINITY);
		outExtents = glm::vec3(0.05f);

		SinglePrecisionTexCoords texCoords{};

		float cmToMMultiplier = cmToM ? 0.01f : 1.0f;
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
			SinglePrecisionTexCoords chunkTexCoords{};
			chunkVertices.SetCount(static_cast<uint32_t>(shape.mesh.positions.size() / 3)); // Divide size by 3 to account for the fact that the array is of floats rather than vector structs.
			chunkTexCoords.SetCount(chunkVertices.Count());

			for (uint32_t j = 0; j < chunkVertices.Count(); ++j)
			{
				// Positions, normals etc are stored in float format, in groups. (3 for positions and normals, 2 for tex coords).
				// Multiply the index to stride to the current float group.
				uint64_t nIndex = static_cast<uint64_t>(j) * 3; // Stride of three floats for positions and normals.
				uint64_t nTexIndex = static_cast<uint64_t>(j) * 2; // Stride of two floats for texture coordinates.

				// Copy attributes...
				if (shape.mesh.positions.size())
					chunkVertices[j].m_position = glm::vec3(shape.mesh.positions[nIndex], shape.mesh.positions[nIndex + 1], shape.mesh.positions[nIndex + 2]) * cmToMMultiplier;
				else
					chunkVertices[j].m_position = glm::vec3(0.0f, 0.0f, 0.0f);

				outOrigin.x = glm::min(outOrigin.x, chunkVertices[j].m_position.x);
				outOrigin.y = glm::min(outOrigin.y, chunkVertices[j].m_position.y);
				outOrigin.z = glm::min(outOrigin.z, chunkVertices[j].m_position.z);

				outExtents.x = glm::max(outExtents.x, chunkVertices[j].m_position.x);
				outExtents.y = glm::max(outExtents.y, chunkVertices[j].m_position.y);
				outExtents.z = glm::max(outExtents.z, chunkVertices[j].m_position.z);

				if (shape.mesh.normals.size())
					chunkVertices[j].m_normal = Vec4PackedHalfFloat(shape.mesh.normals[nIndex], shape.mesh.normals[nIndex + 1], shape.mesh.normals[nIndex + 2], 1.0f);
				else
					chunkVertices[j].m_normal = Vec4PackedHalfFloat(0.0f, 0.0f, 0.0f, 0.0f);

				if (shape.mesh.texcoords.size())
				{
					chunkTexCoords[j] = glm::vec2(shape.mesh.texcoords[nTexIndex], 1.0f - shape.mesh.texcoords[nTexIndex + 1]);
					chunkVertices[j].m_texCoords = Vec2PackedHalfFloat(chunkTexCoords[j].x, chunkTexCoords[j].y);
				}
				else
				{
					chunkTexCoords[j] = glm::vec2(0.0f, 0.0f);
					chunkVertices[j].m_texCoords = Vec2PackedHalfFloat(0.0f, 0.0f);
				}

				// Add the vertex to the whole mesh vertex array.
				vertices.PushBack(chunkVertices[j]);
				texCoords.PushBack(chunkTexCoords[j]);
			}

			// Calculate tangents for each vertex...
			CalculateTangents(chunkVertices, chunkIndices, chunkTexCoords);
		}

		// Calculate tangents for whole mesh...
		CalculateTangents(vertices, indices, texCoords);

		outExtents = outExtents - outOrigin;
	}

	inline void MeshEncoder::BuildMesh(const AssetStatus& asset, const Ctrl::IDataContainer* properties)
	{
		// Array of all vertices of all mesh chunks, for a single mesh VBO.
		VertexBuffer wholeMeshVertices;
		IndexBuffer wholeMeshIndices;
		MeshletBuffer meshlets;

		glm::vec3 meshBoundOrigin;
		glm::vec3 meshBoundExtents;
		LoadOBJ(wholeMeshVertices, wholeMeshIndices, meshBoundOrigin, meshBoundExtents, asset.m_fullPath.c_str(), properties->GetBooleanValue("Mesh.ConvertCmToM"));

		if (properties->GetBooleanValue("Mesh.GenerateMeshlets"))
		{
			// Calculate meshlets...
			{
				Bounds meshBounds(meshBoundOrigin, meshBoundExtents);
				auto maxExtent = glm::max(glm::max(meshBoundExtents.x, meshBoundExtents.y), meshBoundExtents.z);

				glm::vec3 meshCenter = meshBounds.Center();

				Bounds meshletTreeBounds;
				meshletTreeBounds.m_extents = glm::vec3(maxExtent) * 1.001f; // Expand extents by 1 mm to account for precision error.
				meshletTreeBounds.m_origin = meshCenter - (meshletTreeBounds.m_extents * 0.5f);

				assert(meshletTreeBounds.Encapsulates(meshBounds));
				MeshletOctree meshletOctree(meshletTreeBounds, &wholeMeshVertices);

				uint32_t origIndexCount = wholeMeshIndices.Count();
				for (uint32_t i = 0; i < origIndexCount; i += 3)
				{
					meshletOctree.AddTriangle({ glm::uvec3(wholeMeshIndices[i], wholeMeshIndices[i + 1], wholeMeshIndices[i + 2]) });
				}
				wholeMeshIndices.Clear();

				meshletOctree.OutputIndexBuffer(wholeMeshIndices);
				assert(wholeMeshIndices.Count() >= origIndexCount);

				// If the index buffer is not divisible by meshlet size, pad it with invisible point triangles using the last index in the buffer.
				uint32_t meshletModulo = wholeMeshIndices.Count() % MeshletOctree::MeshletIndexSize;
				if (meshletModulo > 0)
				{
					uint32_t oldCount = wholeMeshIndices.Count();
					uint32_t newCount = oldCount + (MeshletOctree::MeshletIndexSize - meshletModulo);

					MeshIndex lastIndex = wholeMeshIndices.Back();
					for (uint32_t i = 0; i < (newCount - oldCount); ++i)
					{
						wholeMeshIndices.PushBack(lastIndex);
					}
				}
			}

			// Gen meshlet data...
			{
				uint32_t meshletCount = wholeMeshIndices.Count() / MeshletOctree::MeshletIndexSize;
				for (uint32_t i = 0; i < meshletCount; ++i)
				{
					Meshlet& meshlet = meshlets.PushBack();

					uint32_t meshletIndexOffset = i * MeshletOctree::MeshletIndexSize;
					Bounds meshletBounds;

					CLib::Vector<glm::vec3, MeshletOctree::MeshletIndexSize> triNormals;
					for (uint32_t j = 0; j < MeshletOctree::MeshletIndexSize; j += 3)
					{
						uint32_t index0 = wholeMeshIndices[meshletIndexOffset + j];
						uint32_t index1 = wholeMeshIndices[meshletIndexOffset + j + 1];
						uint32_t index2 = wholeMeshIndices[meshletIndexOffset + j + 2];

						if (index0 + index1 + index2 == 0)
							break;

						Vertex& vertA = wholeMeshVertices[index0];
						Vertex& vertB = wholeMeshVertices[index1];
						Vertex& vertC = wholeMeshVertices[index2];

						if (j == 0)
						{
							meshletBounds.m_origin = vertA.m_position;
							meshletBounds.m_extents = glm::vec3(0.0f);
						}
						else
						{
							meshletBounds.Encapsulate(vertA.m_position);
						}

						meshletBounds.Encapsulate(vertB.m_position);
						meshletBounds.Encapsulate(vertC.m_position);

						triNormals.PushBack(glm::vec3(vertA.m_normal.Unpack()));
						triNormals.PushBack(glm::vec3(vertB.m_normal.Unpack()));
						triNormals.PushBack(glm::vec3(vertC.m_normal.Unpack()));
					}

					meshlet.m_origin = meshletBounds.m_origin;
					meshlet.m_extents = meshletBounds.m_extents;

					glm::vec3 averageNormal(0.0f);
					for (const glm::vec3& normal : triNormals)
					{
						averageNormal += normal;
					}
					averageNormal = glm::normalize(averageNormal);

					float angularSpan = 0.0f;
					for (const glm::vec3& normal : triNormals)
					{
						float dot = glm::dot(averageNormal, normal);
						angularSpan = glm::max(angularSpan, glm::acos(dot));
					}

					//meshlet.m_normal = Vec4PackedSnorm(glm::vec4(averageNormal, normalTolerance));
					meshlet.m_normalPacked0 = Vec2PackedHalfFloat(averageNormal.x, averageNormal.y);
					meshlet.m_normalPacked1 = Vec2PackedHalfFloat(averageNormal.z, angularSpan);
				}
			}
		}

		uint64_t vertexBufferSize = wholeMeshVertices.Count() * sizeof(Vertex);
		uint64_t indexBufferSize = wholeMeshIndices.Count() * sizeof(MeshIndex);
		uint64_t meshletBufferSize = meshlets.Count() * sizeof(Meshlet);

		uint32_t totalVertexCount = wholeMeshVertices.Count();
		uint32_t totalIndexCount = wholeMeshIndices.Count();

		MeshCacheData* outCacheData;
		size_t totalSize = vertexBufferSize + indexBufferSize + meshletBufferSize;
		uint8_t* data = reinterpret_cast<uint8_t*>(m_dbWriter->AllocateAsset(asset.m_dbPath.c_str(), sizeof(MeshCacheData), totalSize, asset.m_lastModifiedTime, reinterpret_cast<char**>(&outCacheData)));

		outCacheData->m_vertexCount = wholeMeshVertices.Count();
		outCacheData->m_indexCount = wholeMeshIndices.Count();
		outCacheData->m_meshletCount = meshlets.Count();
		outCacheData->m_vertexDataOffset = 0;
		outCacheData->m_indexOffset = outCacheData->m_vertexDataOffset + vertexBufferSize;
		outCacheData->m_meshletDataOffset = outCacheData->m_indexOffset + indexBufferSize;
		outCacheData->m_boundOrigin = glm::vec4(meshBoundOrigin, 0.0f);
		outCacheData->m_boundExtents = glm::vec4(meshBoundExtents, 0.0f);

		memcpy(data, wholeMeshVertices.Data(), vertexBufferSize);
		memcpy(&data[outCacheData->m_indexOffset], wholeMeshIndices.Data(), indexBufferSize);
		memcpy(&data[outCacheData->m_meshletDataOffset], meshlets.Data(), meshletBufferSize);

		printf("%s: Stored encoded mesh %s in database: %s at location: %s\n", m_name.c_str(), asset.m_fullPath.c_str(), m_dbName.c_str(), asset.m_dbPath.c_str());
	}
}