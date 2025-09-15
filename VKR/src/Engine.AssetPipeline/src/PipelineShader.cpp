#include "PipelineShader.h"
#include "Engine.AssetEncoder/ShaderPermutation.h"

namespace AssetPipeline
{
	Shader::Shader(PB::IRenderer* renderer, AssetEncoder::AssetBinaryDatabaseReader* reader, const char* assetName)
	{
		assert(renderer != nullptr);
		assert(reader != nullptr);
		assert(assetName != nullptr);

		std::hash<const char*> pathHash;

		PB::ShaderModuleDesc moduleDesc{};
		moduleDesc.m_key[0] = pathHash(assetName);
		moduleDesc.m_key[1] = 0;

		AssetEncoder::AssetHandle handle(assetName);
		auto assetInfo = reader->GetAssetInfo(handle);
		assert(assetInfo.m_binarySize > 0);

		uint8_t* assetData = (uint8_t*)malloc(assetInfo.m_binarySize);
		reader->GetAssetBinary(handle, assetData);

		AssetEncoder::ShaderHeader header;
		header.Deserialize(assetData);
		assert(header.m_data.m_permutationCount == 1);

		const AssetEncoder::PermutationData* permData = header.m_permutationData;
		const uint8_t* byteCode = header.m_shaderBinaries + permData->m_binaryOffsetBytes;

		moduleDesc.m_size = permData->m_binarySize;
		moduleDesc.m_byteCode = reinterpret_cast<const char*>(byteCode);
		m_module = renderer->GetShaderModuleCache()->GetModule(moduleDesc);

		free(assetData);
		assert(m_module != 0);
	}
};