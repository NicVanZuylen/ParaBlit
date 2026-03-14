#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Shader.h"
#include "Engine.Math/Vector2.h"

namespace Eng
{
	class RenderGraphBuilder;

	class ShadowAccumPass : public RenderGraphBehaviour
	{
	public:

		ShadowAccumPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

		~ShadowAccumPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder, uint32_t shadowmapResolution, Math::Vector2u targetResolution);

		void SetOutputTexture(PB::ITexture* tex);

		void SetMVPBuffer(PB::IBufferObject* buf);

		void SetCascadeViews(PB::UniformBufferView* views, uint32_t viewCount);

	private:

		static constexpr const uint32_t PCFRandomRotationTextureSize = 64;

		static constexpr const uint32_t GaussianKernelSize = 16;
		struct BlurConstants
		{
			float m_depthDiscontinuityThreshold;
			float m_normalDiscontinuityThreshold;
			float m_depthScaleFactor;
			float m_guassianNormPart;
			PB::Float4 m_weights[GaussianKernelSize];
		};

		void GenerateRandomRotationTexture(PB::Float2* pixelValues);

		Math::Vector2u m_targetResolution;
		PB::ITexture* m_outputTexture = nullptr;
		PB::IBufferObject* m_viewConstantsBuffer = nullptr;
		CLib::Vector<PB::UniformBufferView> m_accumUniformViews;

		PB::ITexture* m_randomRotationTexture = nullptr;
		PB::ResourceView m_gBufferSampler = 0;
		PB::ResourceView m_shadowSampler = 0;
		PB::ResourceView m_randomRotationSampler = 0;

		PB::u32 m_depthIndex = 0;
		PB::u32 m_normalIndex = 1;
		PB::u32 m_shadowmapIndex = 2;

		PB::ICommandList* m_reusableCmdList = nullptr;
	};
};