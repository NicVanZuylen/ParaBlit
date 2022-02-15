#include "BloomExtractionPass.h"
#include "RenderGraph.h"
#include "Shader.h"
#include "BlurHelper.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

using namespace PBClient;

BloomExtractionPass::BloomExtractionPass(PB::IRenderer* renderer, CLib::Allocator* allocator, bool halfRes) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
	m_halfRes = halfRes;

	PB::BufferObjectDesc constantsDesc{};
	constantsDesc.m_bufferSize = sizeof(BloomConstants);
	constantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
	m_bloomConstantsBuffer = m_renderer->AllocateBuffer(constantsDesc);

	BloomConstants* bloomConstantsData = reinterpret_cast<BloomConstants*>(m_bloomConstantsBuffer->BeginPopulate());
	bloomConstantsData->m_rgbChannelWeights = { 0.2126f, 0.7152f, 0.0722f };
	bloomConstantsData->m_minBrightnessThreshold = 1.65f;
	m_bloomConstantsBuffer->EndPopulate();

	PB::SamplerDesc colorSamplerDesc{};
	colorSamplerDesc.m_anisotropyLevels = 1.0f;
	colorSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	colorSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	colorSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
	m_colorSampler = renderer->GetSampler(colorSamplerDesc);
}

BloomExtractionPass::~BloomExtractionPass()
{
	if (m_reusableCmdList)
		m_renderer->FreeCommandList(m_reusableCmdList);
	m_renderer->FreeBuffer(m_bloomConstantsBuffer);
}

void BloomExtractionPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	
}

void BloomExtractionPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	auto RecordPass = [&]()
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
			transientTextures[0]->GetDefaultSRV(),
			transientTextures[1]->GetDefaultSIV(),
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
	if (!m_reusableCmdList)
		m_reusableCmdList = RecordPass();
	info.m_commandContext->CmdExecuteList(m_reusableCmdList);
}

void BloomExtractionPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void BloomExtractionPass::AddToRenderGraph(RenderGraphBuilder* builder)
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
	outColorDesc.m_initialUsage = PB::ETextureState::STORAGE;
	outColorDesc.m_usageFlags = outColorDesc.m_initialUsage;

	nodeDesc.m_renderWidth = outColorDesc.m_width;
	nodeDesc.m_renderHeight = outColorDesc.m_height;

	builder->AddNode(nodeDesc);
}
