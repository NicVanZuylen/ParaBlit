#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Shader.h"
#include "Engine.Math/Vector2.h"

namespace Eng
{
	class RenderGraphBuilder;

	class ReflectionBlurPass : public RenderGraphBehaviour
	{
	public:

		ReflectionBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

		~ReflectionBlurPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution);

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

		PB::ICommandList* m_reusableCmdList = nullptr;

		Math::Vector2u m_targetResolution{};
		PB::IBufferObject* m_blurConstants = nullptr;
		PB::ResourceView m_blurImageSampler = 0;
		uint8_t m_blurBufferIndex = 0;
		uint8_t m_blurAuxBufferIndex = 0;
		uint8_t m_depthBufferIndex = 0;
		uint8_t m_roughnessBufferIndex = 0;
	};
};