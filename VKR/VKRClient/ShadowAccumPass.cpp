#include "ShadowAccumPass.h"
#include "RenderGraph.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

#include <random>

using namespace PBClient;

ShadowAccumPass::ShadowAccumPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	PB::BufferObjectDesc blurConstantsDesc{};
	blurConstantsDesc.m_bufferSize = sizeof(BlurConstants);
	blurConstantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
	m_blurConstants = m_renderer->AllocateBuffer(blurConstantsDesc);
	m_blurConstantsView = m_blurConstants->GetViewAsUniformBuffer();

	PB::TextureDesc randomRotationTexDesc{};
	randomRotationTexDesc.m_width = PCFRandomRotationTextureSize;
	randomRotationTexDesc.m_height = PCFRandomRotationTextureSize;
	randomRotationTexDesc.m_usageStates = PB::ETextureState::SAMPLED;
	randomRotationTexDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
	randomRotationTexDesc.m_data.m_format = PB::ETextureFormat::R32G32_FLOAT;
	randomRotationTexDesc.m_data.m_size = sizeof(float) * 2 * PCFRandomRotationTextureSize * PCFRandomRotationTextureSize;
	randomRotationTexDesc.m_data.m_data = m_allocator->Alloc(randomRotationTexDesc.m_data.m_size);
	GenerateRandomRotationTexture(reinterpret_cast<PB::Float2*>(randomRotationTexDesc.m_data.m_data));
	m_randomRotationTexture = m_renderer->AllocateTexture(randomRotationTexDesc);
	m_allocator->Free(randomRotationTexDesc.m_data.m_data);

	// Initial transition of texture to correct state.
	{
		PB::CommandContextDesc scopedContextDesc{};
		scopedContextDesc.m_renderer = m_renderer;
		scopedContextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;
		scopedContextDesc.m_flags = PB::ECommandContextFlags::PRIORITY;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(scopedContextDesc);
		scopedContext->Begin();

		scopedContext->CmdTransitionTexture(m_randomRotationTexture, PB::ETextureState::COPY_DST, PB::ETextureState::SAMPLED);

		scopedContext->End();
		scopedContext->Return();
	}

	PB::SamplerDesc gBufferSamplerDesc;
	gBufferSamplerDesc.m_anisotropyLevels = 1.0f;
	gBufferSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
	m_gBufferSampler = renderer->GetSampler(gBufferSamplerDesc);

	gBufferSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
	m_randomRotationSampler = m_renderer->GetSampler(gBufferSamplerDesc);

	gBufferSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
	gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
	m_blurImageSampler = m_renderer->GetSampler(gBufferSamplerDesc);

	gBufferSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_borderColor = PB::ESamplerBorderColor::WHITE;
	gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_BORDER;
	m_shadowSampler = renderer->GetSampler(gBufferSamplerDesc);
}

ShadowAccumPass::~ShadowAccumPass()
{
	if (m_reusableCmdListA)
		m_renderer->FreeCommandList(m_reusableCmdListA);

	if (m_reusableCmdListB)
		m_renderer->FreeCommandList(m_reusableCmdListB);

	m_renderer->FreeTexture(m_randomRotationTexture);
	m_renderer->FreeBuffer(m_blurConstants);
}

void ShadowAccumPass::OnPrePass(const RenderGraphInfo& info)
{

}

void ShadowAccumPass::OnPassBegin(const RenderGraphInfo& info)
{
	auto RecordPassA = [&]()
	{
		auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth();
		auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight();
		constexpr uint32_t WorkGroupSizeXY = 32;

		PB::CommandContextDesc scopedContextDesc{};
		scopedContextDesc.m_renderer = m_renderer;
		scopedContextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;
		scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(scopedContextDesc);
		scopedContext->Begin(info.m_renderPass, info.m_frameBuffer);

		PB::GraphicsPipelineDesc accumPipelineDesc{};
		accumPipelineDesc.m_attachmentCount = 1;
		accumPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS; // Always should disable depth testing.
		accumPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
		accumPipelineDesc.m_stencilTestEnable = false;
		accumPipelineDesc.m_cullMode = PB::EFaceCullMode::FRONT;
		accumPipelineDesc.m_subpass = 0;
		accumPipelineDesc.m_renderPass = info.m_renderPass;
		accumPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Shader(m_renderer, "TestAssets/Shaders/SPIR-V/vs_screenQuad.spv", m_allocator).GetModule();
		accumPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Shader(m_renderer, "TestAssets/Shaders/SPIR-V/fs_shadow_accum.spv", m_allocator).GetModule();

		PB::Pipeline accumPipeline = m_renderer->GetPipelineCache()->GetPipeline(accumPipelineDesc);
		scopedContext->CmdBindPipeline(accumPipeline);
		scopedContext->SetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
		scopedContext->SetScissor({ 0, 0, renderWidth, renderHeight });

		assert(m_mvpBuffer && m_svbView);
		PB::UniformBufferView uboViews[] { m_mvpBuffer->GetViewAsUniformBuffer(), m_svbView };

		PB::ResourceView resourceViews[]
		{
			info.m_renderTargets[0]->GetDefaultSRV(), // G Depth
			info.m_renderTargets[1]->GetDefaultSRV(), // G Normal
			m_gBufferSampler,
			info.m_renderTargets[2]->GetDefaultSRV(), // Shadow map
			m_shadowSampler,
			m_randomRotationTexture->GetDefaultSRV(),
			m_randomRotationSampler,
		};

		PB::BindingLayout bindings;
		bindings.m_uniformBufferCount = _countof(uboViews);
		bindings.m_uniformBuffers = uboViews;
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

void ShadowAccumPass::OnPostPass(const RenderGraphInfo& info)
{
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[3], PB::ETextureState::COLORTARGET, PB::ETextureState::STORAGE);
	auto RecordPassB = [&]()
	{
		auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth();
		auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight();
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

		PB::ComputePipelineDesc blurPipelineDesc{};
		blurPipelineDesc.m_computeModule = Shader(m_renderer, "TestAssets/Shaders/SPIR-V/cs_shadow_blur_v_fast.spv", m_allocator).GetModule();
		PB::Pipeline verticalBlurPipeline = m_renderer->GetPipelineCache()->GetPipeline(blurPipelineDesc);
		blurPipelineDesc.m_computeModule = Shader(m_renderer, "TestAssets/Shaders/SPIR-V/cs_shadow_blur_h_fast.spv", m_allocator).GetModule();
		PB::Pipeline horizontalBlurPipeline = m_renderer->GetPipelineCache()->GetPipeline(blurPipelineDesc);

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

		PB::ResourceView resourceViews[]
		{
			0,
			info.m_renderTargets[0]->GetDefaultSRV(), // G Depth
			info.m_renderTargets[1]->GetDefaultSRV(), // G Normal
			m_blurImageSampler,
			m_gBufferSampler,
			0,
		};

		PB::ResourceView maskASRV = info.m_renderTargets[3]->GetDefaultSRV();
		PB::ResourceView maskBSRV = info.m_renderTargets[4]->GetDefaultSRV();

		PB::ResourceView maskASIV = info.m_renderTargets[3]->GetDefaultSIV();
		PB::ResourceView maskBSIV = info.m_renderTargets[4]->GetDefaultSIV();

		PB::BindingLayout bindings;
		bindings.m_uniformBufferCount = 1;
		bindings.m_uniformBuffers = &m_blurConstantsView;
		bindings.m_resourceCount = _countof(resourceViews);
		bindings.m_resourceViews = resourceViews;

		// The blur shader uses some of it's work group invocations to store excess off-edge samples of count: (GaussianKernelSize - 1) * 2.
		// This reduces the amount of pixels each work group writes to by that amount, so we divide our screen resolution by that new amount.
		uint32_t tileDim = WorkGroupY - (2 * GaussianKernelSizeMinusOne);
		uint32_t workGroupCountV = (renderHeight / tileDim) + (renderHeight % tileDim > 0 ? 1 : 0);
		uint32_t workGroupCountH = (renderWidth / tileDim) + (renderWidth % tileDim > 0 ? 1 : 0);

		// Vertical blur pass.
		scopedContext->CmdBindPipeline(verticalBlurPipeline);
		resourceViews[0] = maskASRV;
		resourceViews[5] = maskBSIV;
		scopedContext->CmdBindResources(bindings);
		scopedContext->CmdDispatch(renderWidth / WorkGroupX, workGroupCountV, 1);

		scopedContext->CmdTransitionTexture(info.m_renderTargets[4], PB::ETextureState::SAMPLED, PB::ETextureState::STORAGE);

		// Horizontal blur pass.
		scopedContext->CmdBindPipeline(horizontalBlurPipeline);
		resourceViews[0] = maskBSRV;
		resourceViews[5] = maskASIV;
		scopedContext->CmdBindResources(bindings);
		scopedContext->CmdDispatch(workGroupCountH, renderHeight / WorkGroupX, 1);

		scopedContext->CmdTransitionTexture(info.m_renderTargets[4], PB::ETextureState::STORAGE, PB::ETextureState::SAMPLED);

		scopedContext->End();
		return scopedContext->Return();
	};
	if (!m_reusableCmdListB)
		m_reusableCmdListB = RecordPassB();

	info.m_commandContext->CmdExecuteList(m_reusableCmdListB);

	// Transition target back to COLORTARGET state for the next pass.
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[3], PB::ETextureState::STORAGE, PB::ETextureState::COLORTARGET);

	constexpr uint32_t outputTarget = 3;

	if (!m_outputTexture)
		return;

	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[outputTarget], PB::ETextureState::STORAGE, PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT, PB::ETextureState::COPY_DST);

	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(info.m_renderTargets[outputTarget], m_outputTexture);

	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
}

void ShadowAccumPass::AddToRenderGraph(RenderGraphBuilder* builder, uint32_t shadowmapResolution)
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

	PB::ISwapChain* swapchain = m_renderer->GetSwapchain();

	// Depth is needed for shadowmap comparison and retreiving world-space position of texels.
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

	AttachmentDesc& shadowmapReadDesc = nodeDesc.m_attachments[2];
	shadowmapReadDesc.m_format = PB::ETextureFormat::D16_UNORM;
	shadowmapReadDesc.m_width = shadowmapResolution;
	shadowmapReadDesc.m_height = shadowmapResolution;
	shadowmapReadDesc.m_name = "WorldShadowmap";
	shadowmapReadDesc.m_usage = PB::EAttachmentUsage::READ;

	AttachmentDesc& shadowMaskDesc = nodeDesc.m_attachments[3]; // Shouldn't need clearing as it is drawn to using a fullscreen quad.
	shadowMaskDesc.m_format = PB::ETextureFormat::R8_UNORM;
	shadowMaskDesc.m_width = swapchain->GetWidth();
	shadowMaskDesc.m_height = swapchain->GetHeight();
	shadowMaskDesc.m_name = "ShadowMaskA";
	shadowMaskDesc.m_usage = PB::EAttachmentUsage::COLOR;
	shadowMaskDesc.m_flags = EAttachmentFlags::SECONDARY_SAMPLED | EAttachmentFlags::SECONDARY_STORAGE;

	AttachmentDesc& finalShadowMaskDesc = nodeDesc.m_attachments[4];
	finalShadowMaskDesc.m_format = PB::ETextureFormat::R8_UNORM;
	finalShadowMaskDesc.m_width = swapchain->GetWidth();
	finalShadowMaskDesc.m_height = swapchain->GetHeight();
	finalShadowMaskDesc.m_name = nullptr;
	finalShadowMaskDesc.m_usage = PB::EAttachmentUsage::READ;
	finalShadowMaskDesc.m_flags = EAttachmentFlags::SECONDARY_SAMPLED | EAttachmentFlags::SECONDARY_STORAGE;

	nodeDesc.m_attachmentCount = 5;
	nodeDesc.m_renderWidth = depthReadDesc.m_width;
	nodeDesc.m_renderHeight = depthReadDesc.m_height;

	builder->AddNode(nodeDesc);
}

void ShadowAccumPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

void ShadowAccumPass::SetMVPBuffer(PB::IBufferObject* buf)
{
	m_mvpBuffer = buf;
}

void ShadowAccumPass::SetSVBBuffer(PB::UniformBufferView view)
{
	m_svbView = view;
}

void ShadowAccumPass::GenerateRandomRotationTexture(PB::Float2* pixelValues)
{
	std::default_random_engine randEngine;
	std::uniform_real_distribution distribution(0.0f, 1.0f);

	static constexpr const float PI = 3.1415926535f;
	static constexpr const float RevolutionRadians = PI * 2.0f;
	static constexpr const uint32_t PixelCount = PCFRandomRotationTextureSize * PCFRandomRotationTextureSize;
	for (uint32_t i = 0; i < PixelCount; ++i)
	{
		float angle = distribution(randEngine) * RevolutionRadians;
		pixelValues[i].x = glm::cos(angle);
		pixelValues[i].y = glm::sin(angle);
	}
}

//void DeferredLightingPass::GenerateDiskSamples()
//{
//	glm::vec4* sampleVectors = reinterpret_cast<glm::vec4*>(m_diskSampleBuffer->BeginPopulate());
//
//	std::default_random_engine randomEngine(238957205);
//	std::uniform_real_distribution distrib(0.0f, 1.0f);
//
//	static constexpr const uint32_t DiskPartitionCount = 4;
//	float PartitionRadiusSlice = 1.0f / DiskPartitionCount;
//
//	static constexpr const float PI = 3.1415926535;
//	static constexpr const float SampleBiasMultiplier = 0.25f;
//
//	float totalArea = PI;
//	uint32_t currentSample = 0;
//	//uint32_t paritionPointCounts[DiskPartitionCount];
//	for (uint32_t i = 0; i < DiskPartitionCount; ++i)
//	{
//		float partitionCircleRadius = (i + 1) * PartitionRadiusSlice;
//		float prevPartitionCircleRadius = i * PartitionRadiusSlice;
//		float partitionRingArea = (PI * glm::pow(partitionCircleRadius, 2)) - (PI * glm::pow(prevPartitionCircleRadius, 2));
//
//		uint32_t partitionSampleCount = glm::floor((partitionRingArea / PI) * PCFDiskSampleCount);
//		//paritionPointCounts[i] = partitionSampleCount;
//		float partitionSliceAngleRange = (2 * PI) / partitionSampleCount;
//		float partitionRandomAngleBias = (PartitionRadiusSlice * (2 * PI)) * SampleBiasMultiplier;
//		float partitionRandomRadialBias = PartitionRadiusSlice * SampleBiasMultiplier;
//		for (uint32_t j = 0; j < partitionSampleCount; ++j)
//		{
//			uint32_t sampleIdx = currentSample + j;
//			float theta = (j * partitionSliceAngleRange) + partitionRandomAngleBias + (((partitionSliceAngleRange - partitionRandomAngleBias) * distrib(randomEngine)));
//			float randomRadius = prevPartitionCircleRadius + partitionRandomRadialBias + (PartitionRadiusSlice * distrib(randomEngine) - partitionRandomRadialBias);
//			sampleVectors[sampleIdx] = glm::vec4(glm::cos(theta), glm::sin(theta), 0.0f, 0.0f) * randomRadius;
//			assert(glm::length(sampleVectors[sampleIdx]) <= partitionCircleRadius);
//		}
//		currentSample += partitionSampleCount;
//	}
//	
//	//glm::vec2 debugPoints[PCFDiskSampleCount];
//	//memcpy(debugPoints, sampleVectors, sizeof(glm::vec2) * PCFDiskSampleCount);
//
//	m_diskSampleBuffer->EndPopulate();
//}
