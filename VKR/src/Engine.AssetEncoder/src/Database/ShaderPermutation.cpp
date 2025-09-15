#include "Engine.AssetEncoder/ShaderPermutation.h"
#include <cassert>

namespace AssetEncoder
{
	void ShaderHeader::Serialize(
		uint8_t* dstData,
		const PermutationData* permutationData,
		uint32_t permutationCount,
		uint8_t* shaderBinaryBlob,
		size_t shaderBinaryBlobSize
	)
	{
		m_data.m_permutationCount = permutationCount;
		m_data.m_shaderBinarySize = shaderBinaryBlobSize;

		// Fixed header data.
		std::memcpy(dstData, &m_data, FixedHeaderSize);
		dstData += FixedHeaderSize;

		// Permutation data.
		size_t permutationDataSize = GetPermutationDataSize();
		std::memcpy(dstData, permutationData, permutationDataSize);
		dstData += permutationDataSize;

		// Shader binaries.
		std::memcpy(dstData, shaderBinaryBlob, m_data.m_shaderBinarySize);
	}

	void ShaderHeader::Deserialize(const uint8_t* srcData)
	{
		// Fixed header data.
		std::memcpy(&m_data, srcData, FixedHeaderSize);
		srcData += FixedHeaderSize;

		// Validate data.
		assert(m_data.m_version == Version);
		assert(m_data.m_permutationCount >= 1);
		assert(m_data.m_shaderBinarySize > 0);

		// Permutation data.
		size_t permutationDataSize = GetPermutationDataSize();
		m_permutationData = reinterpret_cast<const PermutationData*>(srcData);
		srcData += permutationDataSize;

		// Shader binaries blob.
		m_shaderBinaries = srcData;
	}
};