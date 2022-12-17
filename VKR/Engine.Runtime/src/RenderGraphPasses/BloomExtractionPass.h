#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "World/BlurHelper.h"

namespace Eng
{
	class RenderGraphBuilder;

	class BloomExtractionPass : public RenderGraphBehaviour
	{
	public:

		BloomExtractionPass(PB::IRenderer* renderer, CLib::Allocator* allocator, bool halfRes = false);

		~BloomExtractionPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder);

	private:

		static constexpr const uint32_t BlurTargetMipCount = 5;

		struct BloomConstants
		{
			PB::Float3 m_rgbChannelWeights;
			float m_minBrightnessThreshold;
		};

		PB::IBufferObject* m_bloomConstantsBuffer = nullptr;
		PB::ResourceView m_colorSampler = 0;

		PB::ICommandList* m_reusableCmdList = nullptr;

		bool m_halfRes = false;
	};
};