#include "AmbientOcclusionPass.h"
#include "RenderGraph.h"
#include "Shader.h"
#include "BlurHelper.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

#include <random>

using namespace PBClient;

AmbientOcclusionPass::AmbientOcclusionPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
	m_mvpUBOView = desc.m_mvpUBOView;
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

	PB::BufferObjectDesc aoSampleBufferDesc{};
	aoSampleBufferDesc.m_bufferSize = sizeof(AOConstants);
	aoSampleBufferDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
	m_aoConstantsBuffer = m_renderer->AllocateBuffer(aoSampleBufferDesc);
	m_aoConstantsView = m_aoConstantsBuffer->GetViewAsUniformBuffer();

	PB::TextureDesc randomRotationTexDesc{};
	randomRotationTexDesc.m_width = RandomRotationTextureResolution;
	randomRotationTexDesc.m_height = RandomRotationTextureResolution;
	randomRotationTexDesc.m_usageStates = PB::ETextureState::SAMPLED;
	randomRotationTexDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
	randomRotationTexDesc.m_data.m_format = PB::ETextureFormat::R32G32_FLOAT;
	randomRotationTexDesc.m_data.m_size = sizeof(PB::Float2) * RandomRotationTextureResolution * RandomRotationTextureResolution;
	randomRotationTexDesc.m_data.m_data = m_allocator->Alloc(sizeof(PB::Float2) * (RandomRotationTextureResolution * RandomRotationTextureResolution));

	GenerateRandomRotationTexture(reinterpret_cast<PB::Float2*>(randomRotationTexDesc.m_data.m_data));

	m_randomRotationTexture = m_renderer->AllocateTexture(randomRotationTexDesc);
	m_randomRotationTexView = m_randomRotationTexture->GetDefaultSRV();
	m_allocator->Free(randomRotationTexDesc.m_data.m_data);

	PB::SamplerDesc randomRotationSamplerDesc{};
	randomRotationSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	randomRotationSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	randomRotationSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
	m_randomRotationSampler = m_renderer->GetSampler(randomRotationSamplerDesc);

	m_blurHelper.Init(m_renderer, GaussianKernelSize);
}

AmbientOcclusionPass::~AmbientOcclusionPass()
{
	if (m_reusableCmdListA)
		m_renderer->FreeCommandList(m_reusableCmdListA);

	if (m_reusableCmdListB)
		m_renderer->FreeCommandList(m_reusableCmdListB);

	if (m_aoConstantsBuffer)
		m_renderer->FreeBuffer(m_aoConstantsBuffer);

	if (m_randomRotationTexture)
		m_renderer->FreeTexture(m_randomRotationTexture);

	if(m_blurConstants)
		m_renderer->FreeBuffer(m_blurConstants);
}

void AmbientOcclusionPass::OnPrePass(const RenderGraphInfo& info)
{

}

void AmbientOcclusionPass::OnPassBegin(const RenderGraphInfo& info)
{
	auto RecordPassA = [&]()
	{
		PB::u32 halfResDenom = m_halfRes ? 2u : 1u;

		auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth() / halfResDenom;
		auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight() / halfResDenom;
		
		AOConstants* aoConstants = reinterpret_cast<AOConstants*>(m_aoConstantsBuffer->BeginPopulate());
		aoConstants->m_sampleRadius = 0.1f;
		aoConstants->m_depthBias = 0.001f;
		aoConstants->m_depthSlopeBias = 0.005f;
		aoConstants->m_depthSlopeThreshold = 0.05f;
		aoConstants->m_intensity = 2.0f;
		aoConstants->m_renderWidth = renderWidth;
		aoConstants->m_renderHeight = renderHeight;

		GenerateRandomSamples(aoConstants->m_samples);
		m_aoConstantsBuffer->EndPopulate();

		PB::CommandContextDesc scopedContextDesc{};
		scopedContextDesc.m_renderer = m_renderer;
		scopedContextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;
		scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(scopedContextDesc);
		scopedContext->Begin(info.m_renderPass, info.m_frameBuffer);

		PB::GraphicsPipelineDesc ssaoPipelineDesc{};
		ssaoPipelineDesc.m_attachmentCount = 1;
		ssaoPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
		ssaoPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
		ssaoPipelineDesc.m_stencilTestEnable = false;
		ssaoPipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
		ssaoPipelineDesc.m_subpass = 0;
		ssaoPipelineDesc.m_renderPass = info.m_renderPass;
		ssaoPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = PBClient::Shader(m_renderer, "Shaders/GLSL/vs_screenQuad", m_allocator, true).GetModule();
		ssaoPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = PBClient::Shader(m_renderer, "Shaders/GLSL/fs_ssao", m_allocator, true).GetModule();
		PB::Pipeline ssaoPipeline = m_renderer->GetPipelineCache()->GetPipeline(ssaoPipelineDesc);

		scopedContext->CmdBindPipeline(ssaoPipeline);
		scopedContext->SetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
		scopedContext->SetScissor({ 0, 0, renderWidth, renderHeight });

		PB::UniformBufferView uboBindings[] = { m_mvpUBOView, m_aoConstantsView };
		PB::ResourceView resourceViews[] =
		{
			m_randomRotationTexture->GetDefaultSRV(),
			info.m_renderTargets[0]->GetDefaultSRV(),
			info.m_renderTargets[1]->GetDefaultSRV(),
			m_colorSampler,
			m_randomRotationSampler,
		};

		PB::BindingLayout bindings{};
		bindings.m_uniformBufferCount = _countof(uboBindings);
		bindings.m_uniformBuffers = uboBindings;
		bindings.m_resourceCount = _countof(resourceViews);
		bindings.m_resourceViews = resourceViews;
		scopedContext->CmdBindResources(bindings);

		scopedContext->CmdDraw(6, 1);

		scopedContext->End();
		return scopedContext->Return();
	};

	if (!m_reusableCmdListA)
		m_reusableCmdListA = RecordPassA();
	info.m_commandContext->CmdExecuteList(m_reusableCmdListA);
}

void AmbientOcclusionPass::OnPostPass(const RenderGraphInfo& info)
{
	auto RecordPassB = [&]()
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

		scopedContext->CmdTransitionTexture(info.m_renderTargets[2], PB::ETextureState::COLORTARGET, PB::ETextureState::SAMPLED);

		BlurImageParams blurParams;
		blurParams.m_src = info.m_renderTargets[2];
		blurParams.m_srcMip = 0;
		blurParams.m_buffer0 = info.m_renderTargets[2];
		blurParams.m_buffer0Mip = 0;
		blurParams.m_buffer1 = info.m_renderTargets[3];
		blurParams.m_buffer1Mip = 0;
		blurParams.m_imageFormat = PB::ETextureFormat::R8_UNORM;
		m_blurHelper.Encode(scopedContext.GetContext(), PB::Uint2(renderWidth, renderHeight), blurParams);

		scopedContext->CmdTransitionTexture(info.m_renderTargets[2], PB::ETextureState::SAMPLED, PB::ETextureState::COLORTARGET);

		scopedContext->End();
		return scopedContext->Return();
	};

	if (!m_reusableCmdListB)
		m_reusableCmdListB = RecordPassB();
	info.m_commandContext->CmdExecuteList(m_reusableCmdListB);

	if (!m_outputTexture)
		return;

	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[2], PB::ETextureState::COLORTARGET, PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT, PB::ETextureState::COPY_DST);

	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(info.m_renderTargets[2], m_outputTexture);

	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);

	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[2], PB::ETextureState::COPY_SRC, PB::ETextureState::COLORTARGET);
}

void AmbientOcclusionPass::AddToRenderGraph(RenderGraphBuilder* builder)
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
	nodeDesc.m_computeOnlyPass = false;

	PB::ISwapChain* swapchain = m_renderer->GetSwapchain();
	PB::u32 halfResDenom = m_halfRes ? 2u : 1u;
	PB::u32 renderWidth = swapchain->GetWidth() / halfResDenom;
	PB::u32 renderHeight = swapchain->GetHeight() / halfResDenom;

	AttachmentDesc& depthReadDesc = nodeDesc.m_attachments[0];
	depthReadDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
	depthReadDesc.m_width = swapchain->GetWidth();
	depthReadDesc.m_height = swapchain->GetHeight();
	depthReadDesc.m_name = "G_Depth";
	depthReadDesc.m_usage = PB::EAttachmentUsage::READ;

	AttachmentDesc& normalReadDesc = nodeDesc.m_attachments[1];
	normalReadDesc.m_format = PB::ETextureFormat::R32G32B32A32_FLOAT;
	normalReadDesc.m_width = swapchain->GetWidth();
	normalReadDesc.m_height = swapchain->GetHeight();
	normalReadDesc.m_name = "G_Normal";
	normalReadDesc.m_usage = PB::EAttachmentUsage::READ;

	AttachmentDesc& aoOutDesc = nodeDesc.m_attachments[2];
	aoOutDesc.m_format = PB::ETextureFormat::R8_UNORM;
	aoOutDesc.m_width = renderWidth;
	aoOutDesc.m_height = renderHeight;
	aoOutDesc.m_name = "AO_Output";
	aoOutDesc.m_usage = PB::EAttachmentUsage::COLOR;
	aoOutDesc.m_flags = EAttachmentFlags::COPY_SRC | EAttachmentFlags::SECONDARY_STORAGE | EAttachmentFlags::CLEAR;

	AttachmentDesc& aoBlurBufferDesc = nodeDesc.m_attachments[3];
	aoBlurBufferDesc.m_format = PB::ETextureFormat::R8_UNORM;
	aoBlurBufferDesc.m_width = renderWidth;
	aoBlurBufferDesc.m_height = renderHeight;
	aoBlurBufferDesc.m_name = "AOBlurBuffer";
	aoBlurBufferDesc.m_usage = PB::EAttachmentUsage::STORAGE;
	aoBlurBufferDesc.m_flags = EAttachmentFlags::SECONDARY_SAMPLED;

	nodeDesc.m_attachmentCount = 4;
	nodeDesc.m_renderWidth = renderWidth;
	nodeDesc.m_renderHeight = renderHeight;

	builder->AddNode(nodeDesc);
}

void AmbientOcclusionPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

void AmbientOcclusionPass::GenerateRandomSamples(void* pixelValues)
{
	glm::vec4* samples = reinterpret_cast<glm::vec4*>(pixelValues);

	std::default_random_engine randEngine;
	std::uniform_real_distribution distribution(0.0f, 1.0f);

	for (uint32_t i = 0; i < AOSampleKernelSize; ++i)
	{
		float scale = static_cast<float>(i) / AOSampleKernelSize;

		glm::vec3 sample
		(
			distribution(randEngine) * 2.0f - 1.0f,
			distribution(randEngine) * 2.0f - 1.0f,
			distribution(randEngine)
		);
		sample = glm::normalize(sample);
		sample *= glm::mix<float>(0.1f, 1.0f, glm::pow(scale, 2));
		samples[i] = glm::vec4(sample, 1.0f);
	}
}

void AmbientOcclusionPass::GenerateRandomRotationTexture(PB::Float2* pixelValues)
{
	std::default_random_engine randEngine;
	std::uniform_real_distribution distribution(0.0f, 1.0f);

	static constexpr const float PI = 3.1415926535f;
	static constexpr const float RevolutionRadians = PI * 2.0f;
	static constexpr const uint32_t PixelCount = RandomRotationTextureResolution * RandomRotationTextureResolution;
	for (uint32_t i = 0; i < PixelCount; ++i)
	{
		float angle = distribution(randEngine) * RevolutionRadians;
		pixelValues[i].x = glm::cos(angle);
		pixelValues[i].y = glm::sin(angle);
	}
}
