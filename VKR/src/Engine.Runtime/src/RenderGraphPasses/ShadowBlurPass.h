#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Shader.h"
#include "Engine.Math/Vector2.h"

namespace Eng
{
	class RenderGraphBuilder;

	class ShadowBlurPass : public RenderGraphBehaviour
	{
	public:

		ShadowBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator, bool blurRayTracedShadows);

		~ShadowBlurPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution);

		void SetOutputTexture(PB::ITexture* tex);

	private:

		static constexpr const uint32_t GaussianKernelSize = 16;
		struct BlurConstants
		{
			float m_depthDiscontinuityThreshold;
			float m_depthScaleFactor;
			float m_guassianNormPart;
			float m_pad;
			PB::Float4 m_weights[GaussianKernelSize];
		};

		Math::Vector2u m_targetResolution{};
		PB::ITexture* m_outputTexture = nullptr;
		PB::IBufferObject* m_blurConstants = nullptr;

		PB::ResourceView m_gBufferSampler = 0;
		PB::ResourceView m_blurImageSampler = 0;

		PB::ICommandList* m_reusableCmdList = nullptr;

		bool m_blurRayTracedShadows = false;
	};
};