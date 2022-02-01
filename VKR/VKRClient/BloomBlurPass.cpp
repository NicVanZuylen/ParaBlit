#include "BloomBlurPass.h"
#include "RenderGraph.h"
#include "Shader.h"
#include "BlurHelper.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

using namespace PBClient;

BloomBlurPass::BloomBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator, bool halfRes) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
	m_halfRes = halfRes;

	PB::SamplerDesc blurSamplerDesc{};
	blurSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	blurSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
	blurSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_BORDER;
	m_blurImageSampler = m_renderer->GetSampler(blurSamplerDesc);

	PB::SamplerDesc mergeSamplerDesc{};
	mergeSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	mergeSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
	mergeSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_BORDER;
	mergeSamplerDesc.maxLod = static_cast<float>(BlurTargetMipCount);
	m_mergeSampler = m_renderer->GetSampler(mergeSamplerDesc);

	m_blurHelper.Init(m_renderer, GaussianKernelSize);
}

BloomBlurPass::~BloomBlurPass()
{
	if (m_reusableCmdList)
		m_renderer->FreeCommandList(m_reusableCmdList);
}

void BloomBlurPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	
}

void BloomBlurPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	auto RecordPass = [&]()
	{
		PB::u32 halfResDenom = m_halfRes ? 2u : 1u;

		auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth() / halfResDenom;
		auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight() / halfResDenom;
		constexpr uint32_t GaussianKernelSizeMinusOne = GaussianKernelSize - 1;

		PB::CommandContextDesc scopedContextDesc{};
		scopedContextDesc.m_renderer = m_renderer;
		scopedContextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
		scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(scopedContextDesc);
		scopedContext->Begin();

		for (uint32_t i = 0; i < BlurTargetMipCount; ++i)
		{

			BlurImageParams blurParams;
			blurParams.m_src = transientTextures[1];
			blurParams.m_srcMip = i == 0 ? i : (i - 1);
			blurParams.m_buffer0 = transientTextures[1];
			blurParams.m_buffer0Mip = i;
			blurParams.m_buffer1 = transientTextures[2];
			blurParams.m_buffer1Mip = i;
			blurParams.m_imageFormat = info.m_renderer->GetSwapchain()->GetImageFormat();
			m_blurHelper.Encode(scopedContext.GetContext(), PB::Uint2(renderWidth, renderHeight), blurParams);

			PB::SubresourceRange subresources{};
			subresources.m_baseMip = i;
			subresources.m_mipCount = 1;

			scopedContext->CmdTransitionTexture(transientTextures[1], PB::ETextureState::STORAGE, PB::ETextureState::SAMPLED, subresources);
			scopedContext->CmdTransitionTexture(transientTextures[2], PB::ETextureState::SAMPLED, PB::ETextureState::STORAGE, subresources);
		}

		PB::ComputePipelineDesc mergePipelineDesc{};
		mergePipelineDesc.m_computeModule = Shader(m_renderer, "Shaders/GLSL/cs_merge_bloom", m_allocator, true).GetModule();
		PB::Pipeline mergePipeline = m_renderer->GetPipelineCache()->GetPipeline(mergePipelineDesc);

		PB::TextureViewDesc mergeSrcView{};
		mergeSrcView.m_texture = transientTextures[1];
		mergeSrcView.m_format = info.m_renderer->GetSwapchain()->GetImageFormat();
		mergeSrcView.m_expectedState = PB::ETextureState::SAMPLED;
		mergeSrcView.m_subresources.m_mipCount = BlurTargetMipCount;

		PB::ResourceView mergeResources[]
		{
			transientTextures[0]->GetDefaultSRV(),
			transientTextures[1]->GetView(mergeSrcView),
			m_mergeSampler,
			transientTextures[3]->GetDefaultSIV()
		};

		PB::BindingLayout mergeBindings;
		mergeBindings.m_uniformBufferCount = 0;
		mergeBindings.m_uniformBuffers = nullptr;
		mergeBindings.m_resourceCount = _countof(mergeResources);
		mergeBindings.m_resourceViews = mergeResources;

		constexpr const uint32_t MergeWorkGroupSizeXY = 32;
		uint32_t mergeWorkGroupCountW = ((renderWidth * halfResDenom) / MergeWorkGroupSizeXY) + 1;
		uint32_t mergeWorkGroupCountH = ((renderHeight * halfResDenom) / MergeWorkGroupSizeXY) + 1;

		scopedContext->CmdBindPipeline(mergePipeline);
		scopedContext->CmdBindResources(mergeBindings);
		scopedContext->CmdDispatch(mergeWorkGroupCountW, mergeWorkGroupCountH, 1);

		scopedContext->End();
		return scopedContext->Return();
	};
	if (!m_reusableCmdList)
		m_reusableCmdList = RecordPass();
	info.m_commandContext->CmdExecuteList(m_reusableCmdList);
}

void BloomBlurPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	if (!m_outputTexture)
		return;

	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(transientTextures[3], PB::ETextureState::STORAGE, PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT, PB::ETextureState::COPY_DST);

	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(transientTextures[3], m_outputTexture);

	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
}

void BloomBlurPass::AddToRenderGraph(RenderGraphBuilder* builder)
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

	TransientTextureDesc& colorDesc = nodeDesc.m_transientTextures.PushBackInit();
	colorDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
	colorDesc.m_width = swapchain->GetWidth();
	colorDesc.m_height = swapchain->GetHeight();
	colorDesc.m_name = "LightingColorOutput";
	colorDesc.m_initialUsage = PB::ETextureState::SAMPLED;
	colorDesc.m_usageFlags = colorDesc.m_initialUsage;

	TransientTextureDesc& outColorDesc = nodeDesc.m_transientTextures.PushBackInit();
	outColorDesc.m_format = swapchain->GetImageFormat();
	outColorDesc.m_width = renderWidth;
	outColorDesc.m_height = renderHeight;
	outColorDesc.m_mipCount = BlurTargetMipCount;
	outColorDesc.m_name = "BloomColorOutput";
	outColorDesc.m_initialUsage = PB::ETextureState::SAMPLED;
	outColorDesc.m_finalUsage = PB::ETextureState::STORAGE;
	outColorDesc.m_usageFlags = PB::ETextureState::SAMPLED | PB::ETextureState::STORAGE;

	TransientTextureDesc& blurColorBufferDesc = nodeDesc.m_transientTextures.PushBackInit();
	blurColorBufferDesc.m_format = swapchain->GetImageFormat();
	blurColorBufferDesc.m_width = renderWidth;
	blurColorBufferDesc.m_height = renderHeight;
	blurColorBufferDesc.m_mipCount = BlurTargetMipCount;
	blurColorBufferDesc.m_name = "BloomBlurColor";
	blurColorBufferDesc.m_initialUsage = PB::ETextureState::STORAGE;
	blurColorBufferDesc.m_usageFlags = PB::ETextureState::SAMPLED | PB::ETextureState::STORAGE;

	TransientTextureDesc& mergedOutputDesc = nodeDesc.m_transientTextures.PushBackInit();
	mergedOutputDesc.m_format = swapchain->GetImageFormat();
	mergedOutputDesc.m_width = swapchain->GetWidth();
	mergedOutputDesc.m_height = swapchain->GetHeight();
	mergedOutputDesc.m_name = "MergedOutput";
	mergedOutputDesc.m_initialUsage = PB::ETextureState::STORAGE;
	mergedOutputDesc.m_finalUsage = PB::ETextureState::STORAGE;
	mergedOutputDesc.m_usageFlags = PB::ETextureState::STORAGE;

	nodeDesc.m_renderWidth = outColorDesc.m_width;
	nodeDesc.m_renderHeight = outColorDesc.m_height;

	builder->AddNode(nodeDesc);
}

void BloomBlurPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}
