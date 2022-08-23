#pragma once

#include <cassert>
#include <AssetEncoder/AssetDatabaseReader.h>
#include <IRenderer.h>

namespace AssetPipeline
{
	class Shader
	{
	public:

		Shader(PB::IRenderer* renderer, AssetEncoder::AssetBinaryDatabaseReader* reader, const char* assetName);

		~Shader() = default;

		PB::ShaderModule GetModule() const { return m_module; }

	private:

		PB::ShaderModule m_module{};
	};
};