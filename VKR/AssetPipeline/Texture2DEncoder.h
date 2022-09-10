#pragma once
#include <AssetEncoder/EncoderBase.h>

#include "IRenderer.h"
#include "ICommandContext.h"
#include "Include/AssetPipeline/TextureShared.h"

import AssetPropertyFile;

namespace AssetPipeline
{
	class Texture2DEncoder : public AssetEncoder::EncoderBase
	{
	public:

		Texture2DEncoder(const char* name, const char* dbName, const char* assetDirectory, const char* pipelineDBDir, PB::IRenderer* renderer);

		~Texture2DEncoder();

	private:

		void EncodeUncompressedTexture(const AssetStatus& asset, const AssetPropertyFile& properties);

		void GenerateEnvironmentMap(PB::ICommandContext* cmdContext, PB::ITexture* srcCube, PB::ITexture*& envMap, EConvolutedMapType mapType);

		PB::ITexture* GetAsCube(PB::ICommandContext* cmdContext, PB::ITexture* srcTexture, bool isHdr);

		void EncodeEnvironmentMap(const AssetStatus& asset, const AssetPropertyFile& properties);

		AssetEncoder::AssetBinaryDatabaseReader* m_pipelineDBReader = nullptr;
		PB::IRenderer* m_renderer = nullptr;
		PB::IBufferObject* m_cubeGenConstantsBuffers[6]{};
		PB::IBufferObject* m_cubeConvolutionConstantsBuffers[6]{};
		PB::IBufferObject* m_cubeConvolutionMaterialConstantsBuffers[ConvolutionMipmapCount]{};
		PB::ShaderModule m_cubeGenVertexModule{};
		PB::ShaderModule m_cubeGenFragmentModule{};
		PB::ShaderModule m_irradianceFSModule{};
		PB::ShaderModule m_prefilterFSModule{};
		PB::ShaderModule m_previewVSModule{};
		PB::ShaderModule m_previewFSModule{};
	};
};

