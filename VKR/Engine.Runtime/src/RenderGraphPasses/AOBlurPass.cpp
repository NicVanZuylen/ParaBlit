#include "AOBlurPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Resource/Shader.h"
#include "World/BlurHelper.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

#include <random>

namespace Eng
{
	AOBlurPass::AOBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc) : RenderGraphBehaviour(renderer, allocator)
	{
		m_renderer = renderer;
		m_allocator = allocator;
		m_halfRes = desc.m_halfRes;

		PB::SamplerDesc colorSamplerDesc{};
		colorSamplerDesc.m_anisotropyLevels = 1.0f;
		colorSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
		colorSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
		colorSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
		m_colorSampler = renderer->GetSampler(colorSamplerDesc);

		PB::SamplerDesc blurSamplerDesc{};
		blurSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		blurSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
		blurSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_BORDER;
		m_blurImageSampler = m_renderer->GetSampler(blurSamplerDesc);

		m_blurHelper.Init(m_renderer, GaussianKernelSize);
	}

	AOBlurPass::~AOBlurPass()
	{
		if (m_reusableCmdList)
			m_renderer->FreeCommandList(m_reusableCmdList);
	}

	void AOBlurPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{

	}

	void AOBlurPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		auto RecordPass = [&]()
		{
			PB::u32 halfResDenom = m_halfRes ? 2u : 1u;

			auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth() / halfResDenom;
			auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight() / halfResDenom;

			PB::CommandContextDesc scopedContextDesc{};
			scopedContextDesc.m_renderer = m_renderer;
			scopedContextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
			scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

			PB::SCommandContext scopedContext(m_renderer);
			scopedContext->Init(scopedContextDesc);
			scopedContext->Begin();

			BlurImageParams blurParams;
			blurParams.m_src = transientTextures[0];
			blurParams.m_srcMip = 0;
			blurParams.m_buffer0 = transientTextures[0];
			blurParams.m_buffer0Mip = 0;
			blurParams.m_buffer1 = transientTextures[1];
			blurParams.m_buffer1Mip = 0;
			blurParams.m_imageFormat = PB::ETextureFormat::R8_UNORM;
			m_blurHelper.Encode(scopedContext.GetContext(), PB::Uint2(renderWidth, renderHeight), blurParams);

			scopedContext->End();
			return scopedContext->Return();
		};

		if (!m_reusableCmdList)
			m_reusableCmdList = RecordPass();
		info.m_commandContext->CmdExecuteList(m_reusableCmdList);
	}

	void AOBlurPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{

	}

	void AOBlurPass::AddToRenderGraph(RenderGraphBuilder* builder)
	{
		if (m_reusableCmdList)
		{
			m_renderer->FreeCommandList(m_reusableCmdList);
			m_reusableCmdList = nullptr;
		}

		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = true;
		nodeDesc.m_computeOnlyPass = true;

		PB::ISwapChain* swapchain = m_renderer->GetSwapchain();
		PB::u32 halfResDenom = m_halfRes ? 2u : 1u;
		PB::u32 renderWidth = swapchain->GetWidth() / halfResDenom;
		PB::u32 renderHeight = swapchain->GetHeight() / halfResDenom;

		TransientTextureDesc& aoOutDesc = nodeDesc.m_transientTextures.PushBackInit();
		aoOutDesc.m_format = PB::ETextureFormat::R8_UNORM;
		aoOutDesc.m_width = renderWidth;
		aoOutDesc.m_height = renderHeight;
		aoOutDesc.m_name = "AO_Output";
		aoOutDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		aoOutDesc.m_finalUsage = PB::ETextureState::STORAGE;
		aoOutDesc.m_usageFlags = PB::ETextureState::SAMPLED | PB::ETextureState::STORAGE;

		TransientTextureDesc& aoBlurBufferDesc = nodeDesc.m_transientTextures.PushBackInit();
		aoBlurBufferDesc.m_format = PB::ETextureFormat::R8_UNORM;
		aoBlurBufferDesc.m_width = renderWidth;
		aoBlurBufferDesc.m_height = renderHeight;
		aoBlurBufferDesc.m_name = "AOBlurBuffer";
		aoBlurBufferDesc.m_initialUsage = PB::ETextureState::STORAGE;
		aoBlurBufferDesc.m_finalUsage = PB::ETextureState::SAMPLED;
		aoBlurBufferDesc.m_usageFlags = PB::ETextureState::SAMPLED | PB::ETextureState::STORAGE;

		nodeDesc.m_renderWidth = renderWidth;
		nodeDesc.m_renderHeight = renderHeight;

		builder->AddNode(nodeDesc);
	}
};