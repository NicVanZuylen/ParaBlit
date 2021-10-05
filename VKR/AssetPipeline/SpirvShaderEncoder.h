#pragma once
#include "AssetEncoder/EncoderBase.h"
#include "AssetEncoder/AssetBinaryDatabase.h"

namespace AssetPipeline
{
	class SpirvShaderEncoder : public AssetEncoder::EncoderBase
	{
	public:

		SpirvShaderEncoder(const char* name, const char* dbName, const char* assetDirectory);

		~SpirvShaderEncoder();

	private:

		inline void BuildShader(const AssetStatus& asset);
	};
}

