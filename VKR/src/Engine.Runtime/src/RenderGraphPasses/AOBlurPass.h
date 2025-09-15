#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "WorldRender/BlurHelper.h"
#include "Engine.Math/Vector2.h"

namespace Eng
{
	class RenderGraphBuilder;

	class AOBlurPass : public RenderGraphBehaviour
	{
	public:

		struct CreateDesc
		{

		};

		AOBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc);

		~AOBlurPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution);

	private:

		static constexpr const uint32_t GaussianKernelSize = 4;
		struct BlurConstants
		{
			float m_guassianNormPart;
			float m_pad0[3];
			PB::Float4 m_weights[GaussianKernelSize];
		};

		Eng::BlurHelper m_blurHelper;
		PB::ResourceView m_colorSampler = 0;
		PB::ResourceView m_blurImageSampler = 0;

		PB::ITexture* m_outputTexture = nullptr;
		PB::ICommandList* m_reusableCmdList = nullptr;

		PB::u32 m_renderWidth = 0;
		PB::u32 m_renderHeight = 0;
	};
};