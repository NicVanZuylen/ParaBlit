#ifndef MESH_H
#define MESH_H

#include "CLib/Vector.h"
#include <string>

#pragma warning(push, 0)
#include "glm.hpp"
#pragma warning(pop)

#include "IRenderer.h"
#include "DrawBatch.h"

namespace PBClient
{
	struct Vertex
	{
		glm::vec4 m_position;
		glm::vec4 m_normal;
		glm::vec4 m_tangent;
		glm::vec2 m_texCoords;
	};

	struct MeshCacheData
	{
		uint64_t m_vertexCount;
		uint64_t m_indexCount;
		size_t m_vertexDataOffset;
		size_t m_indexOffset;
	};

	typedef uint32_t MeshIndex;

	class Mesh
	{
	public:

		Mesh() = default;

		Mesh(PB::IRenderer* renderer, const char* filePath, VertexPool* vertexPool = nullptr);

		~Mesh();

		/*
		Description: Constructor logic.
		*/
		void Init(PB::IRenderer* renderer, const char* filePath, VertexPool* vertexPool = nullptr);

		/*
		Description: Load the mesh from a file, and any included materials.
		Param:
			const char* filePath: The path to the .obj mesh file.
		*/
		void Load(const char* filePath);

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
		Description: Get index of the first vertex in the vertex pool (if vertex pool is used).
		Return Type: uint32_t
		*/
		uint32_t FirstVertex() const;

		/*
		Description: Get the name of this mesh, which should be the name of the file.
		Return Type: const std::string&
		*/
		const std::string& GetName() const;

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
		Description: Get the mesh vertex buffer.
		Return Type: const PB::IBufferObject*
		*/
		const PB::IBufferObject* GetVertexBuffer() const;

		/*
		Description: Get the mesh index buffer.
		Return Type: const PB::IBufferObject*
		*/
		const PB::IBufferObject* GetIndexBuffer() const;

	private:

		/*
		Description: Calculate mesh tangents.
		*/
		void CalculateTangents(CLib::Vector<Vertex>& vertices, CLib::Vector<MeshIndex>& indices);

		/*
		Description: Load and convert mesh from an OBJ file.
		Param:
			CLib::Vector<Vertex>& vertices: The array of output vertices.
			CLib::Vector<MeshIndex>& indices: The array of output indices.
			const char* path: The file path of the .obj file to load.
		*/
		void LoadOBJ(CLib::Vector<Vertex>& vertices, CLib::Vector<MeshIndex>& indices, const char* path);

		PB::IRenderer* m_renderer = nullptr;
		PB::IBufferObject* m_vertexBuffer = nullptr;
		PB::IBufferObject* m_indexBuffer = nullptr;
		uint64_t m_totalVertexCount = 0;
		uint64_t m_totalIndexCount = 0;
		const char* m_filePath = nullptr;
		VertexPool* m_vertexPool = nullptr;
		uint64_t m_firstVertexInPool = 0;
		std::string m_name;
		bool m_empty = true;
	};
}

#endif /* MESH_H */