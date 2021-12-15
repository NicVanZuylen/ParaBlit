#include "BloomPass.h"
#include "RenderGraph.h"
#include "Shader.h"
#include "BlurHelper.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

using namespace PBClient;

BloomPass::BloomPass(PB::IRenderer* renderer, CLib::Allocator* allocator, bool halfRes) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
	m_halfRes = halfRes;

	PB::BufferObjectDesc constantsDesc{};
	constantsDesc.m_bufferSize = sizeof(BloomConstants);
	constantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
	m_bloomConstantsBuffer = m_renderer->AllocateBuffer(constantsDesc);
	constantsDesc.m_bufferSize = sizeof(BlurConstants);
	m_blurConstants = m_renderer->AllocateBuffer(constantsDesc);

	BloomConstants* bloomConstantsData = reinterpret_cast<BloomConstants*>(m_bloomConstantsBuffer->BeginPopulate());
	bloomConstantsData->m_rgbChannelWeights = { 0.2126f, 0.7152f, 0.0722f };
	bloomConstantsData->m_minBrightnessThreshold = 1.35f;
	m_bloomConstantsBuffer->EndPopulate();

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

	PB::SamplerDesc mergeSamplerDesc{};
	mergeSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	mergeSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
	mergeSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_BORDER;
	mergeSamplerDesc.maxLod = static_cast<float>(BlurTargetMipCount);
	m_mergeSampler = m_renderer->GetSampler(mergeSamplerDesc);

	m_blurHelper.Init(m_renderer, GaussianKernelSize);
}

BloomPass::~BloomPass()
{
	if (m_reusableCmdListA)
		m_renderer->FreeCommandList(m_reusableCmdListA);

	if (m_reusableCmdListB)
		m_renderer->FreeCommandList(m_reusableCmdListB);

	m_renderer->FreeBuffer(m_bloomConstantsBuffer);
	m_renderer->FreeBuffer(m_blurConstants);
}

void BloomPass::OnPrePass(const RenderGraphInfo& info)
{
	
}

void BloomPass::OnPassBegin(const RenderGraphInfo& info)
{
	auto RecordPassA = [&]()
	{
		PB::u32 halfResDenom = m_halfRes ? 2u : 1u;

		auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth() / halfResDenom;
		auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight() / halfResDenom;
		constexpr uint32_t WorkGroupSizeXY = 32;

		PB::CommandContextDesc scopedContextDesc{};
		scopedContextDesc.m_renderer = m_renderer;
		scopedContextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
		scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(scopedContextDesc);
		scopedContext->Begin();

		PB::ResourceView resources[]
		{
			info.m_renderTargets[0]->GetDefaultSRV(),
			info.m_renderTargets[1]->GetDefaultSIV(),
			m_colorSampler
		};

		PB::ComputePipelineDesc colorExtractionPipelineDesc;
		colorExtractionPipelineDesc.m_computeModule = Shader(m_renderer, "Shaders/GLSL/cs_bloom_color_extraction", m_allocator, true).GetModule();
		PB::Pipeline colorExtractionPipeline = m_renderer->GetPipelineCache()->GetPipeline(colorExtractionPipelineDesc);

		PB::UniformBufferView bloomConstantsView = m_bloomConstantsBuffer->GetViewAsUniformBuffer();
		PB::BindingLayout bindings;
		bindings.m_uniformBufferCount = 1;
		bindings.m_uniformBuffers = &bloomConstantsView;
		bindings.m_resourceCount = _countof(resources);
		bindings.m_resourceViews = resources;

		scopedContext->CmdBindPipeline(colorExtractionPipeline);
		scopedContext->CmdBindResources(bindings);

		uint32_t workGroupCountW = (renderWidth / WorkGroupSizeXY) + 1;
		uint32_t workGroupCountH = (renderHeight / WorkGroupSizeXY) + 1;
		scopedContext->CmdDispatch(workGroupCountW, workGroupCountH, 1);

		scopedContext->End();
		return scopedContext->Return();
	};
	if (!m_reusableCmdListA)
		m_reusableCmdListA = RecordPassA();
	info.m_commandContext->CmdExecuteList(m_reusableCmdListA);

	auto RecordPassB = [&]()
	{
		PB::u32 halfResDenom = m_halfRes ? 2u : 1u;

		auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth() / halfResDenom;
		auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight() / halfResDenom;
		constexpr uint32_t GaussianKernelSizeMinusOne = GaussianKernelSize - 1;
		constexpr uint32_t WorkGroupX = 2;
		constexpr uint32_t WorkGroupY = 512;

		PB::CommandContextDesc scopedContextDesc{};
		scopedContextDesc.m_renderer = m_renderer;
		scopedContextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
		scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(scopedContextDesc);
		scopedContext->Begin();

		{
			PB::SubresourceRange subresources{};
			subresources.m_mipCount = BlurTargetMipCount;

			scopedContext->CmdTransitionTexture(info.m_renderTargets[1], PB::ETextureState::STORAGE, PB::ETextureState::SAMPLED, subresources);
		}

		for (uint32_t i = 0; i < BlurTargetMipCount; ++i)
		{
			BlurImageParams blurParams;
			blurParams.m_src = info.m_renderTargets[1];
			blurParams.m_srcMip = i == 0 ? i : (i - 1);
			blurParams.m_buffer0 = info.m_renderTargets[1];
			blurParams.m_buffer0Mip = i;
			blurParams.m_buffer1 = info.m_renderTargets[2];
			blurParams.m_buffer1Mip = i;
			blurParams.m_imageFormat = info.m_renderer->GetSwapchain()->GetImageFormat();
			m_blurHelper.Encode(scopedContext.GetContext(), PB::Uint2(renderWidth, renderHeight), blurParams);
		}

		PB::ComputePipelineDesc mergePipelineDesc{};
		mergePipelineDesc.m_computeModule = Shader(m_renderer, "Shaders/GLSL/cs_merge_bloom", m_allocator, true).GetModule();
		PB::Pipeline mergePipeline = m_renderer->GetPipelineCache()->GetPipeline(mergePipelineDesc);

		PB::TextureViewDesc mergeSrcView{};
		mergeSrcView.m_texture = info.m_renderTargets[1];
		mergeSrcView.m_format = info.m_renderer->GetSwapchain()->GetImageFormat();
		mergeSrcView.m_expectedState = PB::ETextureState::SAMPLED;
		mergeSrcView.m_subresources.m_mipCount = BlurTargetMipCount;

		PB::ResourceView mergeResources[]
		{
			info.m_renderTargets[0]->GetDefaultSRV(),
			info.m_renderTargets[1]->GetView(mergeSrcView),
			m_mergeSampler,
			info.m_renderTargets[3]->GetDefaultSIV()
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
	if (!m_reusableCmdListB)
		m_reusableCmdListB = RecordPassB();
	info.m_commandContext->CmdExecuteList(m_reusableCmdListB);
}

void BloomPass::OnPostPass(const RenderGraphInfo& info)
{
	if (!m_outputTexture)
		return;

	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[3], PB::ETextureState::STORAGE, PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT, PB::ETextureState::COPY_DST);

	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(info.m_renderTargets[3], m_outputTexture);

	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
}

void BloomPass::AddToRenderGraph(RenderGraphBuilder* builder)
{
	if (m_reusableCmdListA)
	{
		m_renderer->FreeCommandList(m_reusableCmdListA);
		m_reusableCmdListA = nullptr;
	}

	if (m_reusableCmdListB)
	{
		m_renderer->FreeCommandList(m_reusableCmdListB);
		m_reusableCmdListB = nullptr;
	}

	NodeDesc nodeDesc{};
	nodeDesc.m_behaviour = this;
	nodeDesc.m_useReusableCommandLists = true;
	nodeDesc.m_computeOnlyPass = true;

	PB::ISwapChain* swapchain = m_renderer->GetSwapchain();
	PB::u32 halfResDenom = m_halfRes ? 2u : 1u;
	PB::u32 renderWidth = swapchain->GetWidth() / halfResDenom;
	PB::u32 renderHeight = swapchain->GetHeight() / halfResDenom;

	AttachmentDesc& colorDesc = nodeDesc.m_attachments[0];
	colorDesc.m_format = PB::ETextureFormat::R32G32B32A32_FLOAT;
	colorDesc.m_width = swapchain->GetWidth();
	colorDesc.m_height = swapchain->GetHeight();
	colorDesc.m_name = "LightingColorOutput";
	colorDesc.m_usage = PB::EAttachmentUsage::READ;

	AttachmentDesc& outColorDesc = nodeDesc.m_attachments[1];
	outColorDesc.m_format = swapchain->GetImageFormat();
	outColorDesc.m_width = renderWidth;
	outColorDesc.m_height = renderHeight;
	outColorDesc.m_mipCount = BlurTargetMipCount;
	outColorDesc.m_name = "BloomOutColor";
	outColorDesc.m_usage = PB::EAttachmentUsage::STORAGE;
	outColorDesc.m_flags = EAttachmentFlags::COPY_SRC | EAttachmentFlags::SECONDARY_SAMPLED;

	AttachmentDesc& blurColorBufferDesc = nodeDesc.m_attachments[2];
	blurColorBufferDesc.m_format = swapchain->GetImageFormat();
	blurColorBufferDesc.m_width = renderWidth;
	blurColorBufferDesc.m_height = renderHeight;
	blurColorBufferDesc.m_mipCount = BlurTargetMipCount;
	blurColorBufferDesc.m_name = "BloomBlurColor";
	blurColorBufferDesc.m_usage = PB::EAttachmentUsage::STORAGE;
	blurColorBufferDesc.m_flags = EAttachmentFlags::COPY_SRC | EAttachmentFlags::SECONDARY_SAMPLED;

	AttachmentDesc& mergedOutputDesc = nodeDesc.m_attachments[3];
	mergedOutputDesc.m_format = swapchain->GetImageFormat();
	mergedOutputDesc.m_width = swapchain->GetWidth();
	mergedOutputDesc.m_height = swapchain->GetHeight();
	mergedOutputDesc.m_name = "BloomMergedOutput";
	mergedOutputDesc.m_usage = PB::EAttachmentUsage::STORAGE;
	mergedOutputDesc.m_flags = EAttachmentFlags::COPY_SRC | EAttachmentFlags::SECONDARY_SAMPLED;

	nodeDesc.m_attachmentCount = 4;
	nodeDesc.m_renderWidth = outColorDesc.m_width;
	nodeDesc.m_renderHeight = outColorDesc.m_height;

	builder->AddNode(nodeDesc);
}

void BloomPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}
