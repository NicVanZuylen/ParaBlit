#include "ShadowAccumPass.h"
#include "RenderGraph.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

#include <random>

using namespace PBClient;

ShadowAccumPass::ShadowAccumPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	PB::TextureDataDesc randomRotationDataDesc{};
	randomRotationDataDesc.m_size = sizeof(float) * 2 * PCFRandomRotationTextureSize * PCFRandomRotationTextureSize;
	randomRotationDataDesc.m_data = m_allocator->Alloc(randomRotationDataDesc.m_size);

	PB::TextureDesc randomRotationTexDesc{};
	randomRotationTexDesc.m_width = PCFRandomRotationTextureSize;
	randomRotationTexDesc.m_height = PCFRandomRotationTextureSize;
	randomRotationTexDesc.m_usageStates = PB::ETextureState::SAMPLED;
	randomRotationTexDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
	randomRotationTexDesc.m_format = PB::ETextureFormat::R32G32_FLOAT;
	randomRotationTexDesc.m_data = &randomRotationDataDesc;
	GenerateRandomRotationTexture(reinterpret_cast<PB::Float2*>(randomRotationDataDesc.m_data));
	m_randomRotationTexture = m_renderer->AllocateTexture(randomRotationTexDesc);
	m_allocator->Free(randomRotationDataDesc.m_data);

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

	PB::SamplerDesc samplerDesc;
	samplerDesc.m_anisotropyLevels = 1.0f;
	samplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	samplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	samplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
	m_gBufferSampler = renderer->GetSampler(samplerDesc);

	samplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	samplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	samplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
	m_randomRotationSampler = m_renderer->GetSampler(samplerDesc);

	samplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	samplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	samplerDesc.m_borderColor = PB::ESamplerBorderColor::WHITE;
	samplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_BORDER;
	m_shadowSampler = renderer->GetSampler(samplerDesc);
}

ShadowAccumPass::~ShadowAccumPass()
{
	if (m_reusableCmdList)
		m_renderer->FreeCommandList(m_reusableCmdList);

	m_renderer->FreeTexture(m_randomRotationTexture);
}

void ShadowAccumPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void ShadowAccumPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
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
		accumPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Shader(m_renderer, "Shaders/GLSL/vs_screenQuad", m_allocator, true).GetModule();
		accumPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Shader(m_renderer, "Shaders/GLSL/fs_shadow_accum", m_allocator, true).GetModule();

		PB::Pipeline accumPipeline = m_renderer->GetPipelineCache()->GetPipeline(accumPipelineDesc);
		scopedContext->CmdBindPipeline(accumPipeline);
		scopedContext->CmdSetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
		scopedContext->CmdSetScissor({ 0, 0, renderWidth, renderHeight });

		assert(m_mvpBuffer && m_svbView);
		PB::UniformBufferView uboViews[] { m_mvpBuffer->GetViewAsUniformBuffer(), m_svbView };

		PB::ResourceView resourceViews[]
		{
			transientTextures[0]->GetDefaultSRV(), // G Depth
			transientTextures[1]->GetDefaultSRV(), // G Normal
			m_gBufferSampler,
			transientTextures[2]->GetDefaultSRV(), // Shadow map
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
	if (!m_reusableCmdList)
		m_reusableCmdList = RecordPassA();

	info.m_commandContext->CmdExecuteList(m_reusableCmdList);
}

void ShadowAccumPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void ShadowAccumPass::AddToRenderGraph(RenderGraphBuilder* builder, uint32_t shadowmapResolution)
{
	if (m_reusableCmdList)
	{
		m_renderer->FreeCommandList(m_reusableCmdList);
		m_reusableCmdList = nullptr;
	}

	NodeDesc nodeDesc{};
	nodeDesc.m_behaviour = this;
	nodeDesc.m_useReusableCommandLists = true;

	PB::ISwapChain* swapchain = m_renderer->GetSwapchain();

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

	TransientTextureDesc& shadowmapReadDesc = nodeDesc.m_transientTextures.PushBackInit();
	shadowmapReadDesc.m_format = PB::ETextureFormat::D16_UNORM;
	shadowmapReadDesc.m_width = shadowmapResolution;
	shadowmapReadDesc.m_height = shadowmapResolution;
	shadowmapReadDesc.m_name = "WorldShadowmap";
	shadowmapReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
	shadowmapReadDesc.m_usageFlags = shadowmapReadDesc.m_initialUsage;

	AttachmentDesc& shadowMaskDesc = nodeDesc.m_attachments.PushBackInit();
	shadowMaskDesc.m_format = PB::ETextureFormat::R8_UNORM;
	shadowMaskDesc.m_width = swapchain->GetWidth();
	shadowMaskDesc.m_height = swapchain->GetHeight();
	shadowMaskDesc.m_name = "ShadowMask";
	shadowMaskDesc.m_usage = PB::EAttachmentUsage::COLOR;
	shadowMaskDesc.m_flags = EAttachmentFlags::NONE;

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
