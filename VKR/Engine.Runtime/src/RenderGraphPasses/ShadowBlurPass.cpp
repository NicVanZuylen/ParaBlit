#include "ShadowBlurPass.h"
#include "RenderGraph/RenderGraph.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

using namespace PBClient;

ShadowBlurPass::ShadowBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	PB::BufferObjectDesc blurConstantsDesc{};
	blurConstantsDesc.m_bufferSize = sizeof(BlurConstants);
	blurConstantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
	m_blurConstants = m_renderer->AllocateBuffer(blurConstantsDesc);

	PB::SamplerDesc gBufferSamplerDesc;
	gBufferSamplerDesc.m_anisotropyLevels = 1.0f;
	gBufferSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
	m_gBufferSampler = renderer->GetSampler(gBufferSamplerDesc);

	gBufferSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
	gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
	m_blurImageSampler = m_renderer->GetSampler(gBufferSamplerDesc);
}

ShadowBlurPass::~ShadowBlurPass()
{
	if(m_reusableCmdList)
		m_renderer->FreeCommandList(m_reusableCmdList);

	m_renderer->FreeBuffer(m_blurConstants);
}

void ShadowBlurPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void ShadowBlurPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	auto RecordPass = [&]()
	{
		auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth();
		auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight();
		uint32_t dispatchRenderWidth = renderWidth;
		uint32_t dispatchRenderHeight = renderHeight;
		constexpr uint32_t GaussianKernelSizeMinusOne = GaussianKernelSize - 1;
		constexpr uint32_t WorkGroupX = 1;
		constexpr uint32_t WorkGroupY = 256;

		PB::CommandContextDesc scopedContextDesc{};
		scopedContextDesc.m_renderer = m_renderer;
		scopedContextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
		scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(scopedContextDesc);
		scopedContext->Begin();

		constexpr const float TwoPi = 2.0f * 3.14159f;
		constexpr const float KernelSampleCount = (float)GaussianKernelSize;

		BlurConstants* blurConstants = reinterpret_cast<BlurConstants*>(m_blurConstants->BeginPopulate());
		blurConstants->m_depthDiscontinuityThreshold = 0.001f;
		blurConstants->m_normalDiscontinuityThreshold = 0.2f;
		blurConstants->m_depthScaleFactor = 16.0f;

		float sigma = KernelSampleCount / 3.0f;
		float doubleSigmaSqr = 2 * (sigma * sigma);
		for (uint32_t i = 0; i < GaussianKernelSize; ++i)
		{
			float iFloat = static_cast<float>(i);
			blurConstants->m_weights[i].x = glm::exp(-(iFloat * iFloat) / doubleSigmaSqr);
		}
		blurConstants->m_guassianNormPart = 1.0f / (sigma * glm::sqrt(TwoPi)); // First half of the guassian function. Multiplying by this will normalize the blur colour samples.

		m_blurConstants->EndPopulate();


		PB::ComputePipelineDesc blurPipelineDesc{};
		blurPipelineDesc.m_computeModule = Shader(m_renderer, "Shaders/GLSL/cs_shadow_blur_v_fast", m_allocator, true).GetModule();
		PB::Pipeline verticalBlurPipeline = m_renderer->GetPipelineCache()->GetPipeline(blurPipelineDesc);
		blurPipelineDesc.m_computeModule = Shader(m_renderer, "Shaders/GLSL/cs_shadow_blur_h_fast", m_allocator, true).GetModule();
		PB::Pipeline horizontalBlurPipeline = m_renderer->GetPipelineCache()->GetPipeline(blurPipelineDesc);

		PB::ResourceView resourceViews[]
		{
			0,
			transientTextures[0]->GetDefaultSRV(), // G Depth
			transientTextures[1]->GetDefaultSRV(), // G Normal
			m_blurImageSampler,
			m_gBufferSampler,
			0,
		};

		PB::ResourceView maskASRV = transientTextures[2]->GetDefaultSRV();
		PB::ResourceView maskBSRV = transientTextures[3]->GetDefaultSRV();

		PB::ResourceView maskASIV = transientTextures[2]->GetDefaultSIV();
		PB::ResourceView maskBSIV = transientTextures[3]->GetDefaultSIV();

		PB::UniformBufferView blurConstantsView = m_blurConstants->GetViewAsUniformBuffer();
		PB::BindingLayout bindings;
		bindings.m_uniformBufferCount = 1;
		bindings.m_uniformBuffers = &blurConstantsView;
		bindings.m_resourceCount = _countof(resourceViews);
		bindings.m_resourceViews = resourceViews;

		// The blur shader uses some of it's work group invocations to store excess off-edge samples of count: (GaussianKernelSize - 1) * 2.
		// This reduces the amount of pixels each work group writes to by that amount, so we divide our screen resolution by that new amount.
		uint32_t tileDim = WorkGroupY - (2 * GaussianKernelSizeMinusOne);
		uint32_t workGroupCountV = (dispatchRenderHeight / tileDim) + (dispatchRenderHeight % tileDim > 0 ? 1 : 0);
		uint32_t workGroupCountH = (dispatchRenderWidth / tileDim) + (dispatchRenderWidth % tileDim > 0 ? 1 : 0);

		// Vertical blur pass.
		scopedContext->CmdBindPipeline(verticalBlurPipeline);
		resourceViews[0] = maskASRV;
		resourceViews[5] = maskBSIV;
		scopedContext->CmdBindResources(bindings);
		scopedContext->CmdDispatch(dispatchRenderWidth / WorkGroupX, workGroupCountV, 1);

		scopedContext->CmdTransitionTexture(transientTextures[2], PB::ETextureState::SAMPLED, PB::ETextureState::STORAGE);
		scopedContext->CmdTransitionTexture(transientTextures[3], PB::ETextureState::STORAGE, PB::ETextureState::SAMPLED);

		// Horizontal blur pass.
		scopedContext->CmdBindPipeline(horizontalBlurPipeline);
		resourceViews[0] = maskBSRV;
		resourceViews[5] = maskASIV;
		scopedContext->CmdBindResources(bindings);
		scopedContext->CmdDispatch(workGroupCountH, dispatchRenderHeight / WorkGroupX, 1);

		uint32_t totalGroupCountV = (dispatchRenderWidth / WorkGroupX) * workGroupCountV;
		uint32_t totalGroupCountH = (dispatchRenderHeight / WorkGroupX) * workGroupCountH;

		scopedContext->End();
		return scopedContext->Return();
	};

	if (!m_reusableCmdList)
		m_reusableCmdList = RecordPass();

	info.m_commandContext->CmdExecuteList(m_reusableCmdList);
}

void ShadowBlurPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	
}

void ShadowBlurPass::AddToRenderGraph(RenderGraphBuilder* builder)
{
	if (m_reusableCmdList)
	{
		m_renderer->FreeCommandList(m_reusableCmdList);
		m_reusableCmdList = nullptr;
	}

	PB::ISwapChain* swapchain = m_renderer->GetSwapchain();

	NodeDesc nodeDesc{};
	nodeDesc.m_behaviour = this;
	nodeDesc.m_computeOnlyPass = true;
	nodeDesc.m_useReusableCommandLists = true;

	// Depth is needed for shadowmap comparison and retreiving world-space position of texels.
	TransientTextureDesc& depthReadDesc = nodeDesc.m_transientTextures.PushBackInit();
	depthReadDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
	depthReadDesc.m_width = swapchain->GetWidth();
	depthReadDesc.m_height = swapchain->GetHeight();
	depthReadDesc.m_name = "G_Depth";
	depthReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
	depthReadDesc.m_usageFlags = depthReadDesc.m_initialUsage;

	TransientTextureDesc& normalReadDesc = nodeDesc.m_transientTextures.PushBackInit();
	normalReadDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
	normalReadDesc.m_width = swapchain->GetWidth();
	normalReadDesc.m_height = swapchain->GetHeight();
	normalReadDesc.m_name = "G_Normal";
	normalReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
	normalReadDesc.m_usageFlags = normalReadDesc.m_initialUsage;

	TransientTextureDesc& shadowMaskDesc = nodeDesc.m_transientTextures.PushBackInit();
	shadowMaskDesc.m_format = PB::ETextureFormat::R8_UNORM;
	shadowMaskDesc.m_width = swapchain->GetWidth();
	shadowMaskDesc.m_height = swapchain->GetHeight();
	shadowMaskDesc.m_name = "ShadowMask";
	shadowMaskDesc.m_initialUsage = PB::ETextureState::SAMPLED;
	shadowMaskDesc.m_finalUsage = PB::ETextureState::STORAGE;
	shadowMaskDesc.m_usageFlags = shadowMaskDesc.m_initialUsage | PB::ETextureState::STORAGE;

	TransientTextureDesc& shadowMaskBlurBufferDesc = nodeDesc.m_transientTextures.PushBackInit();
	shadowMaskBlurBufferDesc.m_format = PB::ETextureFormat::R8_UNORM;
	shadowMaskBlurBufferDesc.m_width = swapchain->GetWidth();
	shadowMaskBlurBufferDesc.m_height = swapchain->GetHeight();
	shadowMaskBlurBufferDesc.m_name = "ShadowMaskBlurBuffer";
	shadowMaskBlurBufferDesc.m_initialUsage = PB::ETextureState::STORAGE;
	shadowMaskBlurBufferDesc.m_finalUsage = PB::ETextureState::SAMPLED;
	shadowMaskBlurBufferDesc.m_usageFlags = shadowMaskBlurBufferDesc.m_initialUsage | PB::ETextureState::SAMPLED;

	nodeDesc.m_renderWidth = depthReadDesc.m_width;
	nodeDesc.m_renderHeight = depthReadDesc.m_height;
	builder->AddNode(nodeDesc);
}

void ShadowBlurPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}