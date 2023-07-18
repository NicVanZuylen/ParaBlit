#pragma once
#include "Engine.AssetEncoder/EncoderBase.h"
#include "Engine.Control/ISettingsParsers.h"
#include <Engine.ParaBlit/ParaBlitDefs.h>

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include "glm/glm.hpp"
#include "glm/gtc/packing.hpp"
#pragma warning(pop)


namespace PB
{
	class IRenderer;
}

namespace AssetPipeline
{
	class MeshEncoder : public AssetEncoder::EncoderBase
	{
	public:

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

		struct Vec4PackedUnorm
		{
			Vec4PackedUnorm(glm::vec4 vec)
			{
				m_packed = glm::packUnorm3x10_1x2(vec);
			}

			Vec4PackedUnorm(float x, float y, float z, float w)
			{
				m_packed = glm::packUnorm3x10_1x2(glm::vec4(x, y, z, w));
			}

			glm::uint32 m_packed;
		};
		static_assert(sizeof(Vec4PackedUnorm) == sizeof(int32_t));

		struct Vec4PackedSnorm
		{
			Vec4PackedSnorm(glm::vec4 vec)
			{
				m_packed = glm::packSnorm3x10_1x2(vec);
			}

			Vec4PackedSnorm(float x, float y, float z, float w)
			{
				m_packed = glm::packSnorm3x10_1x2(glm::vec4(x, y, z, w));
			}

			glm::uint32 m_packed;
		};
		static_assert(sizeof(Vec4PackedSnorm) == sizeof(int32_t));

		struct Vertex
		{
			glm::vec3 m_position;
			Vec2PackedHalfFloat m_texCoords;
			Vec4PackedHalfFloat m_normal;
			Vec4PackedHalfFloat m_tangent;
		};
		static_assert(sizeof(Vertex) == 32);

		struct Meshlet
		{
			glm::vec3 m_origin;
			Vec2PackedHalfFloat m_normalPacked0;
			glm::vec3 m_extents;
			Vec2PackedHalfFloat m_normalPacked1;
		};

		struct MeshCacheData
		{
			uint64_t m_vertexCount;
			uint64_t m_indexCount;
			uint64_t m_meshletCount;
			size_t m_vertexDataOffset;
			size_t m_indexOffset;
			size_t m_meshletDataOffset;
			glm::vec4 m_boundOrigin;
			glm::vec4 m_boundExtents;
		};

		typedef uint32_t MeshIndex;
		typedef CLib::Vector<Vertex, 1, 1024> VertexBuffer;
		typedef CLib::Vector<MeshIndex, 1, 4096> IndexBuffer;
		typedef CLib::Vector<Meshlet, 1, 256> MeshletBuffer;
		typedef CLib::Vector<glm::vec2, 1, 1024> SinglePrecisionTexCoords;

		MeshEncoder(const char* name, const char* dbName, const char* assetDirectory, PB::IRenderer* renderer);

		~MeshEncoder();

	private:

		inline void CalculateTangents(VertexBuffer& vertices, IndexBuffer& indices, SinglePrecisionTexCoords& texCoords);

		inline void LoadOBJ(VertexBuffer& vertices, IndexBuffer& indices, glm::vec3& outOrigin, glm::vec3& outExtents, const char* path, bool cmToM);

		inline void BuildMesh(const AssetStatus& asset, const Ctrl::IDataContainer* properties);

		PB::IRenderer* m_renderer = nullptr;
		AssetEncoder::AssetBinaryDatabaseReader* m_pipelineDBReader = nullptr;
		PB::ShaderModule m_previewVSModule{};
		PB::ShaderModule m_previewFSModule{};
	};
}
