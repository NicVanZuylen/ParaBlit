#pragma once
#include "Engine.AssetEncoder/EncoderBase.h"

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include "glm/glm.hpp"
#include "glm/gtc/packing.hpp"
#pragma warning(pop)

namespace AssetPipeline
{
	class MeshEncoder : public AssetEncoder::EncoderBase
	{
	public:

		MeshEncoder(const char* name, const char* dbName, const char* assetDirectory);

		~MeshEncoder();

	private:

		struct Vec2PackedHalfFloat
		{
			Vec2PackedHalfFloat(float x, float y) 
			{
				m_packed = glm::packHalf2x16(glm::vec2(x, y));
			}

			Vec2PackedHalfFloat(glm::vec2 vec)
			{
				m_packed = glm::packHalf2x16(vec);
			}

			glm::vec2 Unpack()
			{
				return glm::unpackHalf2x16(m_packed);
			}

			glm::uint m_packed;
		};
		static_assert(sizeof(Vec2PackedHalfFloat) == sizeof(uint32_t));

		struct Vec4PackedHalfFloat
		{
			Vec4PackedHalfFloat(float x, float y, float z, float w)
			{
				m_packed[0] = Vec2PackedHalfFloat(x, y);
				m_packed[1] = Vec2PackedHalfFloat(z, w);
			}

			glm::vec4 Unpack()
			{
				return glm::vec4(m_packed[0].Unpack(), m_packed[1].Unpack());
			}

			Vec2PackedHalfFloat m_packed[2]{ { 0.0f, 0.0f }, { 0.0f, 0.0f } };
		};
		static_assert(sizeof(Vec4PackedHalfFloat) == sizeof(uint64_t));

		struct Vec3PackedUnorm
		{
			Vec3PackedUnorm(glm::vec4 vec)
			{
				m_packed = glm::packUnorm3x10_1x2(vec);
			}

			Vec3PackedUnorm(float x, float y, float z, float w)
			{
				m_packed = glm::packUnorm3x10_1x2(glm::vec4(x, y, z, w));
			}

			glm::uint32 m_packed;
		};
		static_assert(sizeof(Vec3PackedUnorm) == sizeof(int32_t));

		struct Vertex
		{
			glm::vec3 m_position;
			Vec2PackedHalfFloat m_texCoords;
			Vec4PackedHalfFloat m_normal;
			Vec4PackedHalfFloat m_tangent;
		};
		static_assert(sizeof(Vertex) == 32);

		struct MeshCacheData
		{
			uint64_t m_vertexCount;
			uint64_t m_indexCount;
			size_t m_vertexDataOffset;
			size_t m_indexOffset;
			glm::vec4 m_boundOrigin;
			glm::vec4 m_boundExtents;
		};

		typedef uint32_t MeshIndex;
		typedef CLib::Vector<Vertex, 1, 1024> VertexBuffer;
		typedef CLib::Vector<MeshIndex, 1, 4096> IndexBuffer;
		typedef CLib::Vector<glm::vec2, 1, 1024> SinglePrecisionTexCoords;

		inline void CalculateTangents(VertexBuffer& vertices, IndexBuffer& indices, SinglePrecisionTexCoords& texCoords);

		inline void LoadOBJ(VertexBuffer& vertices, IndexBuffer& indices, glm::vec3& outOrigin, glm::vec3& outExtents, const char* path);

		inline void BuildMesh(const AssetStatus& asset);
	};
}
