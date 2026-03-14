#pragma once
#include "Engine.Control/IDataClass.h"
#include "MeshShared.h"
#include "MeshAsset.h"
#include "Engine.AssetEncoder/EncoderBase.h"
#include "Engine.Control/IDataFile.h"

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

		struct Vec2PackedUint16
		{
			Vec2PackedUint16(uint16_t x, uint16_t y)
			{
				m_packed = uint32_t(y) << 16;
				m_packed |= uint32_t(x);
			}

			Vec2PackedUint16(glm::u16vec2 vec)
			{
				m_packed = uint32_t(vec.y) << 16;
				m_packed |= uint32_t(vec.x);
			}

			glm::u16vec2 Unpack()
			{
				return glm::u16vec2(m_packed & 0xFFFF, m_packed >> 16);
			}

			glm::uint m_packed;
		};
		static_assert(sizeof(Vec2PackedUint16) == sizeof(uint32_t));

		struct Vec4PackedHalfFloat
		{
			Vec4PackedHalfFloat(float x, float y, float z, float w)
			{
				m_packed[0] = Vec2PackedHalfFloat(x, y);
				m_packed[1] = Vec2PackedHalfFloat(z, w);
			}

			Vec4PackedHalfFloat(uint64_t packed64)
			{
				m_packed64 = packed64;
			}

			glm::vec4 Unpack()
			{
				return glm::vec4(m_packed[0].Unpack(), m_packed[1].Unpack());
			}

			union
			{
				Vec2PackedHalfFloat m_packed[2]{ { 0.0f, 0.0f }, { 0.0f, 0.0f } };
				uint64_t m_packed64;
			};
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

		typedef CLib::Vector<Vertex, 1, 1024> VertexBuffer;
		typedef CLib::Vector<MeshIndex, 1, 4096> IndexBuffer;
		typedef CLib::Vector<Meshlet, 1, 1024> MeshletBuffer;
		typedef CLib::Vector<glm::vec2, 1, 1024> SinglePrecisionTexCoords;

		MeshEncoder(const char* name, const char* dbName, const char* assetDirectory);

		~MeshEncoder();

	private:

		struct MeshletTriangle
		{
			uint32_t i0 : 10;
			uint32_t i1 : 10;
			uint32_t i2 : 10;
		};

		inline void CalculateNormals(VertexBuffer& vertices, IndexBuffer& indices);
		inline void CalculateTangents(VertexBuffer& vertices, IndexBuffer& indices, SinglePrecisionTexCoords& texCoords);

		inline void LoadOBJ(VertexBuffer& vertices, IndexBuffer& indices, glm::vec3& outOrigin, glm::vec3& outExtents, const char* path, bool cmToM);

		inline void BuildMesh(const AssetStatus& asset, Ctrl::TObjectPtr<MeshAsset> assetData);
	};
}
