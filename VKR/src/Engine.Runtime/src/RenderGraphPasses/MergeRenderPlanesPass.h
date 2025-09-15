#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Shader.h"

#include <string>

namespace Eng
{
	class RenderGraphBuilder;

	class MergeRenderPlanesPass : public RenderGraphBehaviour
	{
	public:

		struct Desc
		{
			const char* srcTextureName = nullptr;
			PB::ETextureFormat srcFormat = PB::ETextureFormat::R8_UNORM;
			uint32_t srcChannelCount = 4;
			PB::ITexture* dstTexture = nullptr;
			PB::ESamplerFilter upsampleMethod = PB::ESamplerFilter::BILINEAR;
			PB::Rect srcRect{};
			PB::Rect dstRect{};
		};

		MergeRenderPlanesPass(Desc& desc, PB::IRenderer* renderer, CLib::Allocator* allocator);

		~MergeRenderPlanesPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder);

	private:

		struct MergeConstants
		{
			uint32_t srcX = 0;
			uint32_t srcY = 0;
			uint32_t srcWidth = 0;
			uint32_t srcHeight = 0;
		};

		void SetupPipeline(PB::RenderPass renderPass);

		PB::Pipeline m_pipeline = 0;
		PB::ResourceView m_sampler = 0;
		PB::IBufferObject* m_mergeConstants = nullptr;
		std::string m_outputName;

		uint8_t m_srcTextureIndex = 0;

		Desc m_desc{};
	};
};