#include "MeshEncoder.h"
#include "../Bounds.h"
#include "meshoptimizer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../TinyObjLoader/tiny_obj_loader.h"

#include <chrono>
#include <cassert>

// For some reason the DirectXMesh header includes min & max macros, which conflict with glm::min/max.
#undef min
#undef max

using namespace Eng::Math;

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
			Ctrl::IDataFile* propertyDataFile = Ctrl::IDataFile::Create();
			Ctrl::IDataFile::EFileStatus fileStatus = propertyDataFile->Open(asset.m_propertyFilePath.c_str(), Ctrl::IDataFile::EOpenMode::OPEN_READ_WRITE, true);
			Ctrl::IDataNode* meshNode = propertyDataFile->GetRoot()->GetOrAddDataNode("Mesh");
			if (asset.m_hasPropertyFile == false || fileStatus == Ctrl::IDataFile::EFileStatus::CANT_OPEN)
			{
				meshNode->SetBool("ConvertCmToM", false);
				meshNode->SetBool("GenerateMeshlets", true);
				meshNode->SetBool("GenerateBLASData", true);
				meshNode->SetInteger("LODCount", DefaultLODCount);
				meshNode->SetInteger("MaxLOD", DefaultLODCount - 1);

				bool skipLODs[MaxLODCount]{};

				meshNode->SetBool("SkipLODLevels", skipLODs, MaxLODCount);

				propertyDataFile->WriteData();
			}

			if (asset.m_outdated)
			{
				BuildMesh(asset, meshNode);
				FlagAsModified();
			}
			else
			{
				WriteUnmodifiedAsset(asset);
			}

			propertyDataFile->Close();
			Ctrl::IDataFile::Destroy(propertyDataFile);
		}
	}

	MeshEncoder::~MeshEncoder()
	{

	}

	inline void MeshEncoder::CalculateNormals(VertexBuffer& vertices, IndexBuffer& indices)
	{
		uint32_t nIndexCount = indices.Count();

		for (uint32_t i = 0; i < nIndexCount; i += 3) // Step 3 indices, as this should run once per triangle
		{
			// Normals must be calculated per triangle. We find the correct vertices per tri by using the indices.

			// Positions
			Vector3f pos1 = vertices[indices[i]].m_position;
			Vector3f pos2 = vertices[indices[i + 1]].m_position;
			Vector3f pos3 = vertices[indices[i + 2]].m_position;

			Vector3f dPos1 = pos2 - pos1;
			Vector3f dPos2 = pos3 - pos1;

			Vector3f normal = Eng::Math::Cross(dPos1, dPos2);

			// Assign tangents to vertices.
			vertices[indices[i]].m_normal = Vec4PackedHalfFloat(normal.x, normal.y, normal.z, 1.0f).m_packed64;
			vertices[indices[i + 1]].m_normal = Vec4PackedHalfFloat(normal.x, normal.y, normal.z, 1.0f).m_packed64;
			vertices[indices[i + 2]].m_normal = Vec4PackedHalfFloat(normal.x, normal.y, normal.z, 1.0f).m_packed64;
		}
	}

	void MeshEncoder::CalculateTangents(VertexBuffer& vertices, IndexBuffer& indices, SinglePrecisionTexCoords& texCoords)
	{
		uint32_t nIndexCount = indices.Count();

		for (uint32_t i = 0; i < nIndexCount; i += 3) // Step 3 indices, as this should run once per triangle
		{
			// Tangents must be calculated per triangle. We find the correct vertices per tri by using the indices.

			// Positions
			Vector3f pos1 = vertices[indices[i]].m_position;
			Vector3f pos2 = vertices[indices[i + 1]].m_position;
			Vector3f pos3 = vertices[indices[i + 2]].m_position;

			// Tex coords
			Vector2f tex1 = texCoords[indices[i]];
			Vector2f tex2 = texCoords[indices[i + 1]];
			Vector2f tex3 = texCoords[indices[i + 2]];

			// Delta positions
			Vector3f dPos1 = pos2 - pos1;
			Vector3f dPos2 = pos3 - pos1;

			// Delta tex coords
			Vector2f dTex1 = tex2 - tex1;
			Vector2f dTex2 = tex3 - tex1;

			float f = 1.0f / (dTex1.x * dTex2.y - dTex2.x * dTex1.y);

			if (tex1.x + tex1.y + tex2.x + tex2.y + tex3.x + tex3.y == 0.0f)
			{
				dTex1 = Vector2f(1.0f, 1.0f);
				dTex2 = Vector2f(1.0f, 1.0f);
				f = 1.0f;
			}

			// Calculate tangent only. The bitangent will be calculated in the vertex shader.
			glm::vec4 tangent;
			tangent.x = f * (dTex2.y * dPos1.x - dTex1.y * dPos2.x);
			tangent.y = f * (dTex2.y * dPos1.y - dTex1.y * dPos2.y);
			tangent.z = f * (dTex2.y * dPos1.z - dTex1.y * dPos2.z);
			tangent = glm::normalize(tangent);

			// Assign tangents to vertices.
			vertices[indices[i]].m_tangent = Vec4PackedHalfFloat(tangent.x, tangent.y, tangent.z, 1.0f).m_packed64;
			vertices[indices[i + 1]].m_tangent = Vec4PackedHalfFloat(tangent.x, tangent.y, tangent.z, 1.0f).m_packed64;
			vertices[indices[i + 2]].m_tangent = Vec4PackedHalfFloat(tangent.x, tangent.y, tangent.z, 1.0f).m_packed64;
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
		size_t totalNormalCount = 0;
		size_t totalIndexCount = 0;

		// Determine size of whole mesh and allocate memory.
		for (uint32_t i = 0; i < chunkCount; ++i)
		{
			totalVertexCount += shapes[i].mesh.positions.size();
			totalNormalCount += shapes[i].mesh.normals.size();
			totalIndexCount += shapes[i].mesh.indices.size();
		}

		totalVertexCount /= 3; // Divide by 3 since positions are floats rather than vec3s.
		totalNormalCount /= 3;

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
				{
					chunkVertices[j].m_position = glm::vec3(shape.mesh.positions[nIndex], shape.mesh.positions[nIndex + 1], shape.mesh.positions[nIndex + 2]) * cmToMMultiplier;
				}
				else
				{
					chunkVertices[j].m_position = glm::vec3(0.0f, 0.0f, 0.0f);
				}

				outOrigin.x = glm::min(outOrigin.x, chunkVertices[j].m_position.x);
				outOrigin.y = glm::min(outOrigin.y, chunkVertices[j].m_position.y);
				outOrigin.z = glm::min(outOrigin.z, chunkVertices[j].m_position.z);

				outExtents.x = glm::max(outExtents.x, chunkVertices[j].m_position.x);
				outExtents.y = glm::max(outExtents.y, chunkVertices[j].m_position.y);
				outExtents.z = glm::max(outExtents.z, chunkVertices[j].m_position.z);

				if (shape.mesh.normals.size())
				{
					chunkVertices[j].m_normal = Vec4PackedHalfFloat(shape.mesh.normals[nIndex], shape.mesh.normals[nIndex + 1], shape.mesh.normals[nIndex + 2], 1.0f).m_packed64;
				}

				if (shape.mesh.texcoords.size())
				{
					chunkTexCoords[j] = glm::vec2(shape.mesh.texcoords[nTexIndex], 1.0f - shape.mesh.texcoords[nTexIndex + 1]);
					chunkVertices[j].m_texCoords = Vec2PackedHalfFloat(chunkTexCoords[j].x, chunkTexCoords[j].y).m_packed;
				}
				else
				{
					chunkTexCoords[j] = glm::vec2(0.0f, 0.0f);
					chunkVertices[j].m_texCoords = Vec2PackedHalfFloat(0.0f, 0.0f).m_packed;
				}

				// Add the vertex to the whole mesh vertex array.
				vertices.PushBack(chunkVertices[j]);
				texCoords.PushBack(chunkTexCoords[j]);
			}

			if (shape.mesh.normals.empty())
			{
				CalculateNormals(chunkVertices, chunkIndices);
			}

			// Calculate tangents for each vertex...
			CalculateTangents(chunkVertices, chunkIndices, chunkTexCoords);
		}

		if (totalNormalCount == 0)
		{
			CalculateNormals(vertices, indices);
		}

		// Calculate tangents for whole mesh...
		CalculateTangents(vertices, indices, texCoords);

		outExtents = outExtents - outOrigin;
	}

	inline void MeshEncoder::BuildMesh(const AssetStatus& asset, const Ctrl::IDataNode* properties)
	{
		// Array of all vertices of all mesh chunks, for a single mesh VBO.
		VertexBuffer wholeMeshVertices;
		IndexBuffer wholeMeshIndices;
		MeshletBuffer meshlets;

		glm::vec3 meshBoundOrigin;
		glm::vec3 meshBoundExtents;
		LoadOBJ(wholeMeshVertices, wholeMeshIndices, meshBoundOrigin, meshBoundExtents, asset.m_fullPath.c_str(), properties->GetBool("ConvertCmToM"));

		// LOD Generation
		//{
		//	const float targetError = 1e-2f;
		//	const uint32_t lodCount = std::clamp<uint32_t>(properties->GetInteger("LodCount", DefaultLODCount), 1, MaxLODCount);
		//	IndexBuffer prevLodIndexBuffer = wholeMeshIndices;
		//	for (uint32_t lod = 1; lod < lodCount; ++lod)
		//	{
		//		printf_s("%s: Generating mesh: [%s] LOD: [%u]...\n", m_name.c_str(), asset.m_dbPath.c_str(), lod);

		//		IndexBuffer lodIndexBuffer;

		//		lodIndexBuffer.Reserve(prevLodIndexBuffer.Count());
		//		size_t simplifiedIndexCount = meshopt_simplify<MeshIndex>
		//		(
		//			lodIndexBuffer.Data(),
		//			prevLodIndexBuffer.Data(),
		//			prevLodIndexBuffer.Count(),
		//			reinterpret_cast<const float*>(wholeMeshVertices.Data()),
		//			wholeMeshVertices.Count(),
		//			sizeof(Vertex),
		//			prevLodIndexBuffer.Count() / 32,
		//			targetError
		//		);
		//		lodIndexBuffer.SetCount(simplifiedIndexCount);

		//		prevLodIndexBuffer = lodIndexBuffer;
		//	}
		//}

		const uint32_t lodCount = std::clamp<uint32_t>(properties->GetInteger("LodCount", DefaultLODCount), 1, MaxLODCount);
		const float targetError = 1e-2f;

		IndexBuffer blasIndices;

		CLib::Vector<MeshletTriangle, 0, 1024> wholeMeshPrimitiveIndices;
		CLib::Vector<uint32_t> lodMeshletEndOffsets;
		CLib::Vector<uint32_t> lodBlasIndexEndOffsets;
		if (properties->GetBool("GenerateMeshlets"))
		{
			constexpr uint32_t VerticesPerMeshlet = 32;
			constexpr uint32_t PrimitivesPerMeshlet = 32;

			IndexBuffer prevLodIndices = wholeMeshIndices;
			wholeMeshIndices.Clear();

			for (uint32_t lod = 0; lod < lodCount; ++lod)
			{
				IndexBuffer lodIndices(prevLodIndices.Count());

				if (lod > 0)
				{
					size_t simplifiedIndexCount = meshopt_simplify<MeshIndex>
					(
						lodIndices.Data(),
						prevLodIndices.Data(),
						prevLodIndices.Count(),
						reinterpret_cast<const float*>(wholeMeshVertices.Data()),
						wholeMeshVertices.Count(),
						sizeof(Vertex),
						prevLodIndices.Count() / LODComplexityReductionFactor,
						targetError
					);

					lodIndices.SetCount(simplifiedIndexCount);
					prevLodIndices = lodIndices;
				}
				else
				{
					lodIndices = prevLodIndices;
				}

				if (properties->GetBool("GenerateBLASData", true) == true)
				{
					blasIndices += lodIndices;
					lodBlasIndexEndOffsets.PushBack(blasIndices.Count());
				}

				size_t maxMeshlets = meshopt_buildMeshletsBound(lodIndices.Count(), VerticesPerMeshlet, PrimitivesPerMeshlet);
				std::vector<meshopt_Meshlet> mOptMeshlets(maxMeshlets);
				std::vector<uint32_t> mOptVertices(maxMeshlets * VerticesPerMeshlet);
				std::vector<uint8_t> mOptTriangles(maxMeshlets * PrimitivesPerMeshlet * 3);

				size_t meshletCount = meshopt_buildMeshlets
				(
					mOptMeshlets.data(),
					mOptVertices.data(),
					mOptTriangles.data(),
					lodIndices.Data(),
					lodIndices.Count(),
					reinterpret_cast<float*>(wholeMeshVertices.Data()),
					wholeMeshVertices.Count(),
					sizeof(Vertex),
					VerticesPerMeshlet,
					PrimitivesPerMeshlet,
					0.0f /* cone_weight */
				);

				const meshopt_Meshlet& lastMeshlet = mOptMeshlets[meshletCount - 1];
				mOptVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
				mOptTriangles.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));
				mOptMeshlets.resize(meshletCount);

				// Gen meshlet data...
				{
					// With MeshOptimizer, the per-meshlet sets output triangles (within mOptTriangles) are in single-index elements which are padded to be divisible by 4.
					// Since we're converting to 32-bit packed triangle indices (MeshletTriangle) we can do so and ignore the padding.
					for (auto& m : mOptMeshlets)
					{
						meshopt_optimizeMeshlet(&mOptVertices[m.vertex_offset], &mOptTriangles[m.triangle_offset], m.triangle_count, m.vertex_count);

						uint32_t mOptTriOffset = m.triangle_offset;
						m.triangle_offset = wholeMeshPrimitiveIndices.Count(); // Offset should be in elements of MeshletTriangle.

						for (uint32_t triIdx = 0; triIdx < m.triangle_count; ++triIdx)
						{
							uint32_t triIdxOffset = mOptTriOffset + (triIdx * 3);

							MeshletTriangle tri{};
							tri.i0 = mOptTriangles[triIdxOffset + 0];
							tri.i1 = mOptTriangles[triIdxOffset + 1];
							tri.i2 = mOptTriangles[triIdxOffset + 2];

							wholeMeshPrimitiveIndices.PushBack(tri);
						}
					}

					for(auto& m : mOptMeshlets)
					{
						uint32_t lodVertexOffset = m.vertex_offset + wholeMeshIndices.Count();
						uint32_t lodPrimitiveOffset = m.triangle_offset;

						Meshlet& nvMeshlet = meshlets.PushBack();
						nvMeshlet.m_vertOffsetCountPacked = lodVertexOffset & 0x3FFFFFF;
						nvMeshlet.m_vertOffsetCountPacked |= (m.vertex_count << VertexOffsetBits);
						nvMeshlet.m_primOffsetCountPacked = lodPrimitiveOffset & 0x3FFFFFF;
						nvMeshlet.m_primOffsetCountPacked |= (m.triangle_count << PrimitiveOffsetBits);

						assert(m.vertex_count <= VerticesPerMeshlet);
						assert(m.triangle_count <= PrimitivesPerMeshlet);

						Bounds meshletBounds;
						CLib::Vector<glm::vec3, VerticesPerMeshlet> triNormals;
						for (uint32_t i = 0; i < m.vertex_count; ++i)
						{
							uint32_t vertIndex = mOptVertices[m.vertex_offset + i];
							Vertex& vert = wholeMeshVertices[vertIndex];

							if (i == 0)
							{
								meshletBounds.m_origin = vert.m_position;
								meshletBounds.m_extents = glm::vec3(0.0f);
							}
							else
							{
								meshletBounds.Encapsulate(vert.m_position);
							}

							triNormals.PushBack(glm::vec3(Vec4PackedHalfFloat(vert.m_normal).Unpack()));
						}

						nvMeshlet.m_center = meshletBounds.Center();
						nvMeshlet.m_radius = glm::max(glm::max(meshletBounds.m_extents.x, meshletBounds.m_extents.y), meshletBounds.m_extents.z) * 0.75f;

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

						nvMeshlet.m_normalDataXYPacked = Vec2PackedHalfFloat(averageNormal.x, averageNormal.y).m_packed;
						nvMeshlet.m_normalDataZThetaPacked = Vec2PackedHalfFloat(averageNormal.z, angularSpan).m_packed;
					}

					// mOptVertices replaces the index buffer for mesh shaders.
					// Concatenate indices.
					{
						uint32_t count = wholeMeshIndices.Count();
						wholeMeshIndices.SetCount(count + mOptVertices.size());
						std::memcpy(wholeMeshIndices.Data() + count, mOptVertices.data(), mOptVertices.size() * sizeof(uint32_t));
					}
				}

				lodMeshletEndOffsets.PushBack(meshlets.Count());
			}

			assert(wholeMeshIndices.Count() < 0x3FFFFFF && "Maximum index count of 67,108,863 exceeded. Please lower the complexity of your mesh.");
			assert(wholeMeshPrimitiveIndices.Count() < 0x3FFFFFF && "Maximum primitive count of 67,108,863 exceeded. Please lower the complexity of your mesh.");
		}

		uint64_t vertexBufferSize = wholeMeshVertices.Count() * sizeof(Vertex);
		uint64_t indexBufferSize = wholeMeshIndices.Count() * sizeof(MeshIndex);
		uint64_t meshletBufferSize = meshlets.Count() * sizeof(Meshlet);
		uint64_t meshletPrimitiveBufferSize = wholeMeshPrimitiveIndices.Count() * sizeof(MeshletTriangle);
		uint64_t blasIndexDataSize = blasIndices.Count() * sizeof(MeshIndex);

		uint32_t totalVertexCount = wholeMeshVertices.Count();
		uint32_t totalIndexCount = wholeMeshIndices.Count();

		MeshCacheData* outCacheData;
		size_t totalSize = vertexBufferSize + indexBufferSize + meshletBufferSize + meshletPrimitiveBufferSize + blasIndexDataSize;
		uint8_t* data = reinterpret_cast<uint8_t*>(m_dbWriter->AllocateAsset(asset.m_dbPath.c_str(), sizeof(MeshCacheData), totalSize, asset.m_lastModifiedTime, reinterpret_cast<char**>(&outCacheData)));

		outCacheData->m_vertexCount = wholeMeshVertices.Count();
		outCacheData->m_indexCount = wholeMeshIndices.Count();
		outCacheData->m_meshletCount = meshlets.Count();
		outCacheData->m_meshletPrimitiveCount = wholeMeshPrimitiveIndices.Count();
		outCacheData->m_blasIndexCount = blasIndices.Count();
		outCacheData->m_vertexDataOffset = 0;
		outCacheData->m_indexOffset = outCacheData->m_vertexDataOffset + vertexBufferSize;
		outCacheData->m_meshletDataOffset = outCacheData->m_indexOffset + indexBufferSize;
		outCacheData->m_meshletPrimitiveDataOffset = outCacheData->m_meshletDataOffset + meshletBufferSize;
		outCacheData->m_blasIndexBufferDataOffset = outCacheData->m_meshletPrimitiveDataOffset + meshletPrimitiveBufferSize;
		outCacheData->m_boundOrigin = Vector4f(meshBoundOrigin.x, meshBoundOrigin.y, meshBoundOrigin.z, 0.0f);
		outCacheData->m_boundExtents = Vector4f(meshBoundExtents.x, meshBoundExtents.y, meshBoundExtents.z, 0.0f);
		outCacheData->m_lodCount = uint16_t(lodCount);
		outCacheData->m_maxLOD = uint8_t(properties->GetInteger("MaxLOD", DefaultLODCount - 1));

		bool skipLODs[MaxLODCount]{};
		{
			uint32_t propValueCount;
			const bool* skipLODsProperty = properties->GetBool("SkipLODLevels", propValueCount);
			if (skipLODsProperty)
			{
				std::memcpy(skipLODs, skipLODsProperty, propValueCount);
			}
		}

		uint8_t lodMask = 0;
		for (uint8_t i = 0; i < MaxLODCount; ++i)
		{
			lodMask |= (uint8_t(skipLODs[i] == true) << i);
		}
		outCacheData->m_lodMask = lodMask;

		for (uint8_t i = 0; i < MaxLODCount; ++i)
		{
			if (i < lodMeshletEndOffsets.Count())
			{
				outCacheData->m_lodMeshletEndOffsets[i] = lodMeshletEndOffsets[i];
			}
			else
			{
				outCacheData->m_lodMeshletEndOffsets[i] = 0;
			}

			if (i < lodBlasIndexEndOffsets.Count())
			{
				outCacheData->m_lodBLASIndexEndOffsets[i] = lodBlasIndexEndOffsets[i];
			}
			else
			{
				outCacheData->m_lodBLASIndexEndOffsets[i] = 0;
			}
		}

		memcpy(data, wholeMeshVertices.Data(), vertexBufferSize);
		memcpy(&data[outCacheData->m_indexOffset], wholeMeshIndices.Data(), indexBufferSize);
		memcpy(&data[outCacheData->m_meshletDataOffset], meshlets.Data(), meshletBufferSize);
		memcpy(&data[outCacheData->m_meshletPrimitiveDataOffset], wholeMeshPrimitiveIndices.Data(), meshletPrimitiveBufferSize);
		memcpy(&data[outCacheData->m_blasIndexBufferDataOffset], blasIndices.Data(), blasIndexDataSize);

		printf("%s: Stored encoded mesh %s in database: %s at location: %s\n", m_name.c_str(), asset.m_fullPath.c_str(), m_dbName.c_str(), asset.m_dbPath.c_str());
	}
}