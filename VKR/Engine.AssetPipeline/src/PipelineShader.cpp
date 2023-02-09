#include "PipelineShader.h"

namespace AssetPipeline
{
	Shader::Shader(PB::IRenderer* renderer, AssetEncoder::AssetBinaryDatabaseReader* reader, const char* assetName)
	{
		assert(renderer != nullptr);
		assert(reader != nullptr);
		assert(assetName != nullptr);

		PB::ShaderModuleDesc moduleDesc{};
		moduleDesc.m_key = assetName;
		moduleDesc.m_keySize = strlen(assetName);

		AssetEncoder::AssetHandle handle(assetName);
		auto assetInfo = reader->GetAssetInfo(handle);
		assert(assetInfo.m_binarySize > 0);

		void* byteCode = malloc(assetInfo.m_binarySize);
		reader->GetAssetBinary(handle, byteCode);

		moduleDesc.m_size = assetInfo.m_binarySize;
		moduleDesc.m_byteCode = reinterpret_cast<const char*>(byteCode);
		m_module = renderer->GetShaderModuleCache()->GetModule(moduleDesc);

		free(byteCode);
		assert(m_module != 0);
	}
};