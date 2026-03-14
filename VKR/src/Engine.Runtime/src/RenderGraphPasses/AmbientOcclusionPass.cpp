#include "AmbientOcclusionPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Resource/Shader.h"

#include <random>

#include <Engine.Math/Scalar.h>
#include <Engine.Math/Vectors.h>

namespace Eng
{
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

		PB::BufferObjectDesc aoSampleBufferDesc{};
		aoSampleBufferDesc.m_bufferSize = sizeof(AOConstants);
		aoSampleBufferDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
		m_aoConstantsBuffer = m_renderer->AllocateBuffer(aoSampleBufferDesc);
		m_aoConstantsView = m_aoConstantsBuffer->GetViewAsUniformBuffer();

		PB::TextureDataDesc randomRotationDataDesc{};
		randomRotationDataDesc.m_data = m_allocator->Alloc(sizeof(PB::Float2) * (RandomRotationTextureResolution * RandomRotationTextureResolution));
		randomRotationDataDesc.m_size = sizeof(PB::Float2) * RandomRotationTextureResolution * RandomRotationTextureResolution;

		PB::TextureDesc randomRotationTexDesc{};
		randomRotationTexDesc.m_width = RandomRotationTextureResolution;
		randomRotationTexDesc.m_height = RandomRotationTextureResolution;
		randomRotationTexDesc.m_usageStates = PB::ETextureState::SAMPLED;
		randomRotationTexDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
		randomRotationTexDesc.m_format = PB::ETextureFormat::R32G32_FLOAT;
		randomRotationTexDesc.m_data = &randomRotationDataDesc;

		GenerateRandomRotationTexture(reinterpret_cast<PB::Float2*>(randomRotationDataDesc.m_data));

		m_randomRotationTexture = m_renderer->AllocateTexture(randomRotationTexDesc);
		m_randomRotationTexView = m_randomRotationTexture->GetDefaultSRV();
		m_allocator->Free(randomRotationDataDesc.m_data);

		PB::SamplerDesc randomRotationSamplerDesc{};
		randomRotationSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
		randomRotationSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
		randomRotationSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
		m_randomRotationSampler = m_renderer->GetSampler(randomRotationSamplerDesc);
	}

	AmbientOcclusionPass::~AmbientOcclusionPass()
	{
		if (m_reusableCmdList)
			m_renderer->FreeCommandList(m_reusableCmdList);

		if (m_aoConstantsBuffer)
			m_renderer->FreeBuffer(m_aoConstantsBuffer);

		if (m_randomRotationTexture)
			m_renderer->FreeTexture(m_randomRotationTexture);
	}

	void AmbientOcclusionPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("AmbientOcclusionPass", { 1.0f, 1.0f, 1.0f, 1.0f });
	}

	void AmbientOcclusionPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		auto RecordPassA = [&]()
		{
			PB::u32 halfResDenom = m_halfRes ? 2u : 1u;

			auto renderWidth = m_targetResolution.x / halfResDenom;
			auto renderHeight = m_targetResolution.y / halfResDenom;

			AOConstants* aoConstants = reinterpret_cast<AOConstants*>(m_aoConstantsBuffer->BeginPopulate());
			aoConstants->m_sampleRadius = 0.05f;
			aoConstants->m_depthBias = 0.001f;
			aoConstants->m_depthSlopeBias = 0.05f;
			aoConstants->m_depthSlopeThreshold = 0.01f;
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
			ssaoPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/vs_screenQuad", 0, m_allocator, true).GetModule();
			ssaoPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_ssao", 0, m_allocator, true).GetModule();
			PB::Pipeline ssaoPipeline = m_renderer->GetPipelineCache()->GetPipeline(ssaoPipelineDesc);

			scopedContext->CmdBindPipeline(ssaoPipeline);
			scopedContext->CmdSetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
			scopedContext->CmdSetScissor({ 0, 0, renderWidth, renderHeight });

			PB::UniformBufferView uboBindings[] = { m_mvpUBOView, m_aoConstantsView };
			PB::ResourceView resourceViews[] =
			{
				m_randomRotationTexture->GetDefaultSRV(),
				transientTextures[0]->GetDefaultSRV(),
				transientTextures[1]->GetDefaultSRV(),
				m_colorSampler,
				m_randomRotationSampler,
			};

			PB::BindingLayout bindings{};
			bindings.m_uniformBufferCount = PB_ARRAY_LENGTH(uboBindings);
			bindings.m_uniformBuffers = uboBindings;
			bindings.m_resourceCount = PB_ARRAY_LENGTH(resourceViews);
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

	void AmbientOcclusionPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdEndLastLabel();
	}

	void AmbientOcclusionPass::AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution)
	{
		if (m_reusableCmdList)
		{
			m_renderer->FreeCommandList(m_reusableCmdList);
			m_reusableCmdList = nullptr;
		}

		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = true;
		nodeDesc.m_computeOnlyPass = false;

		PB::u32 halfResDenom = m_halfRes ? 2u : 1u;
		PB::u32 renderWidth = targetResolution.x / halfResDenom;
		PB::u32 renderHeight = targetResolution.y / halfResDenom;

		TransientTextureDesc& depthReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		depthReadDesc.m_format = PB::ETextureFormat::D32_FLOAT;
		depthReadDesc.m_width = targetResolution.x;
		depthReadDesc.m_height = targetResolution.y;
		depthReadDesc.m_name = "G_Depth";
		depthReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		depthReadDesc.m_usageFlags = depthReadDesc.m_initialUsage;

		TransientTextureDesc& normalReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		normalReadDesc.m_format = PB::ETextureFormat::A2R10G10B10_UNORM;
		normalReadDesc.m_width = targetResolution.x;
		normalReadDesc.m_height = targetResolution.y;
		normalReadDesc.m_name = "G_Normal";
		normalReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		normalReadDesc.m_usageFlags = depthReadDesc.m_initialUsage;

		AttachmentDesc& aoOutDesc = nodeDesc.m_attachments.PushBackInit();
		aoOutDesc.m_format = PB::ETextureFormat::R8_UNORM;
		aoOutDesc.m_width = renderWidth;
		aoOutDesc.m_height = renderHeight;
		aoOutDesc.m_name = "AO_Output";
		aoOutDesc.m_usage = PB::EAttachmentUsage::COLOR;
		aoOutDesc.m_flags = EAttachmentFlags::NONE;

		nodeDesc.m_renderWidth = renderWidth;
		nodeDesc.m_renderHeight = renderHeight;
		m_targetResolution = targetResolution;

		builder->AddNode(nodeDesc);
	}

	void AmbientOcclusionPass::SetOutputTexture(PB::ITexture* tex)
	{
		m_outputTexture = tex;
	}

	void AmbientOcclusionPass::GenerateRandomSamples(void* pixelValues)
	{
		Math::Vector4f* samples = reinterpret_cast<Math::Vector4f*>(pixelValues);

		std::default_random_engine randEngine;
		std::uniform_real_distribution distribution(0.0f, 1.0f);

		for (uint32_t i = 0; i < AOSampleKernelSize; ++i)
		{
			float scale = static_cast<float>(i) / AOSampleKernelSize;

			Math::Vector3f sample
			(
				distribution(randEngine) * 2.0f - 1.0f,
				distribution(randEngine) * 2.0f - 1.0f,
				distribution(randEngine)
			);
			sample = Math::Normalize(sample);
			sample *= Math::Lerp(0.1f, 1.0f, Math::Pow(scale, 2));
			samples[i] = Math::Vector4f(sample, 1.0f);
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
			pixelValues[i].x = Math::Cos(angle);
			pixelValues[i].y = Math::Sin(angle);
		}
	}

};