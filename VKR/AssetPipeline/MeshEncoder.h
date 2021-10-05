#pragma once
#include "AssetEncoder/EncoderBase.h"

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include "glm/glm.hpp"
#pragma warning(pop)

namespace AssetPipeline
{
	class MeshEncoder : public AssetEncoder::EncoderBase
	{
	public:

		MeshEncoder(const char* name, const char* dbName, const char* assetDirectory);

		~MeshEncoder();

	private:

		struct Vertex
		{
			glm::vec4 m_position;
			glm::vec4 m_normal;
			glm::vec4 m_tangent;
			glm::vec2 m_texCoords;
			glm::vec2 m_pad0;
		};

		struct MeshCacheData
		{
			uint64_t m_vertexCount;
			uint64_t m_indexCount;
			size_t m_vertexDataOffset;
			size_t m_indexOffset;
		};

		typedef uint32_t MeshIndex;
		typedef CLib::Vector<Vertex, 1, 1024> VertexBuffer;
		typedef CLib::Vector<MeshIndex, 1, 4096> IndexBuffer;

		inline void CalculateTangents(VertexBuffer& vertices, IndexBuffer& indices);

		inline void LoadOBJ(VertexBuffer& vertices, IndexBuffer& indices, const char* path);

		inline void BuildMesh(const AssetStatus& asset);
	};
}
