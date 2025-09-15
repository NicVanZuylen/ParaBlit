#pragma once
#include "Engine.AssetEncoder/AssetEncoderLib.h"
#include <string>

namespace AssetEncoder
{
	enum class EDefaultPermutationID
	{
		PERMUTATION_0,
		PERMUTATION_1,
		PERMUTATION_3,
		PERMUTATION_4,
		PERMUTATION_5,
		PERMUTATION_6,
		PERMUTATION_7,
		PERMUTATION_8,
		PERMUTATION_END
	};

	enum class EShaderStagePermutation : uint8_t
	{
		VERTEX = 0,
		FRAGMENT = 1,
		COMPUTE = 2,
		TASK = 3,
		MESH = 4
	};

	enum class ERTShaderStagePermutation : uint8_t
	{
		RAYGEN = 0,
		MISS = 1,
		CLOSESTHIT = 2,
		ANYHIT = 3,
		INTERSECTION = 4
	};

	using PermutationKey = uint64_t;

	template<typename PermutationID = EDefaultPermutationID>
	struct ShaderPermutationTable
	{
		static_assert(uint64_t(PermutationID::PERMUTATION_END) > 0 && "PermutationID must have at least one valid permutation.");
		static_assert(uint64_t(PermutationID::PERMUTATION_END) <= 8 && "PermutationID must have 8 or less valid permutations.");

		static constexpr uint8_t PermutationInvalid = ~uint8_t(0);
		static constexpr uint8_t PermutationMaxBits = 8;

		struct PermutationKeyEntry
		{
			uint8_t m_permutationIndex = 0;
		} m_permutationKey[uint64_t(PermutationID::PERMUTATION_END)];

		inline ShaderPermutationTable<PermutationID>& SetPermutation(PermutationID id, uint8_t index)
		{
			m_permutationKey[uint32_t(id)].m_permutationIndex = index;
			return *this;
		}

		inline ShaderPermutationTable<PermutationID>& SetPermutation(PermutationID id, EShaderStagePermutation stage)
		{
			m_permutationKey[uint32_t(id)].m_permutationIndex = uint8_t(stage);
			return *this;
		}

		inline ShaderPermutationTable<PermutationID>& SetPermutation(PermutationID id, ERTShaderStagePermutation stage)
		{
			m_permutationKey[uint32_t(id)].m_permutationIndex = uint8_t(stage);
			return *this;
		}

		ShaderPermutationTable& Reset()
		{
			std::memset(m_permutationKey, 0, sizeof(m_permutationKey));
			return *this;
		}

		PermutationKey GetKey() const
		{
			PermutationKey outKey = PermutationKey(0);

			for (uint32_t i = 0; i < uint32_t(PermutationID::PERMUTATION_END); ++i)
			{
				auto& entry = m_permutationKey[i];

				PermutationKey permutationBits = PermutationKey(entry.m_permutationIndex) << (i * PermutationMaxBits);
				outKey |= permutationBits;
			}
			return outKey;
		}
	};

	struct PermutationData
	{
		PermutationKey m_key = 0;
		size_t m_binaryOffsetBytes = 0;
		size_t m_binarySize = 0;
	};

	struct ShaderHeader
	{
		static constexpr uint32_t Version = 0;

		struct
		{
			uint32_t m_version = Version;
			uint32_t m_permutationCount = 0;
			size_t m_shaderBinarySize = 0;
		} m_data;

		// These are only used to access data from deserialization.
		const PermutationData* m_permutationData = nullptr;
		const uint8_t* m_shaderBinaries = nullptr;

		static constexpr size_t FixedHeaderOffset = 0;
		static constexpr size_t PermutationDataOffset = sizeof(m_data);
		static constexpr size_t FixedHeaderSize = PermutationDataOffset;

		inline size_t GetPermutationDataSize() const { return m_data.m_permutationCount * sizeof(PermutationData); }
		inline size_t GetBinaryDataSize() const { return m_data.m_shaderBinarySize; }
		inline size_t GetTotalDataSize() const 
		{ 
			return FixedHeaderSize + GetPermutationDataSize() + GetBinaryDataSize();
		}
		inline size_t GetByteCodeOffset() const { return FixedHeaderSize + GetPermutationDataSize(); }

		inline const PermutationData* GetPermutation(uint32_t index) const { return m_permutationData + index; }

		ASSET_ENCODER_API void Serialize
		(
			uint8_t* dstData,
			const PermutationData* permutationData, 
			uint32_t permutationCount, 
			uint8_t* shaderBinaryBlob,
			size_t shaderBinaryBlobSize
		);
		ASSET_ENCODER_API void Deserialize(const uint8_t* srcData);
	};
}