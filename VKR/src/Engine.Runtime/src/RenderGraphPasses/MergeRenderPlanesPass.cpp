#include "MergeRenderPlanesPass.h"
#include "RenderGraph/RenderGraph.h"

namespace Eng
{
	MergeRenderPlanesPass::MergeRenderPlanesPass(Desc& desc, PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
	{
		m_desc = desc;
		m_renderer = renderer;
		m_allocator = allocator;

		PB::BufferObjectDesc constantsDesc{};
		constantsDesc.m_name = "MergeRenderPlanesPass_Constants";
		constantsDesc.m_bufferSize = sizeof(MergeConstants);
		constantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
		constantsDesc.m_options = 0;
		m_mergeConstants = m_renderer->AllocateBuffer(constantsDesc);

		MergeConstants mergeConstants{};
		mergeConstants.srcX = desc.srcRect.x;
		mergeConstants.srcY = desc.srcRect.y;
		mergeConstants.srcWidth = desc.srcRect.w;
		mergeConstants.srcHeight = desc.srcRect.h;

		m_mergeConstants->Populate(reinterpret_cast<PB::u8*>(&mergeConstants), sizeof(MergeConstants));
	}

	MergeRenderPlanesPass::~MergeRenderPlanesPass()
	{
		if (m_mergeConstants)
		{
			m_renderer->FreeBuffer(m_mergeConstants);
			m_mergeConstants = nullptr;
		}
	}

	void MergeRenderPlanesPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		
	}

	void MergeRenderPlanesPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("MergeRenderPlanesPass", { 1.0f, 1.0f, 1.0f, 1.0f });

		if (!m_pipeline)
		{
			SetupPipeline(info.m_renderPass);
		}

		if (!m_sampler)
		{
			PB::SamplerDesc samplerDesc{};
			samplerDesc.m_filter = m_desc.upsampleMethod;
			samplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;

			m_sampler = m_renderer->GetSampler(samplerDesc);
		}

		info.m_commandContext->CmdBindPipeline(m_pipeline);

		PB::ResourceView resources[]
		{
			transientTextures[m_srcTextureIndex]->GetDefaultSRV(),
			m_sampler
		};

		PB::UniformBufferView constantsView = m_mergeConstants->GetViewAsUniformBuffer();

		PB::BindingLayout bindings{};
		bindings.m_uniformBufferCount = 1;
		bindings.m_uniformBuffers = &constantsView;
		bindings.m_resourceCount = _countof(resources);
		bindings.m_resourceViews = resources;

		info.m_commandContext->CmdBindResources(bindings);
		info.m_commandContext->CmdSetViewport(m_desc.dstRect, 0.0f, 1.0f);
		info.m_commandContext->CmdSetScissor(m_desc.dstRect);
		info.m_commandContext->CmdDraw(6, 1);

		info.m_commandContext->CmdEndLastLabel();
	}

	void MergeRenderPlanesPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		
	}

	void MergeRenderPlanesPass::AddToRenderGraph(RenderGraphBuilder* builder)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = false;
		nodeDesc.m_computeOnlyPass = false;
		nodeDesc.m_renderWidth = m_desc.dstRect.w;
		nodeDesc.m_renderHeight = m_desc.dstRect.h;

		m_srcTextureIndex = uint8_t(nodeDesc.m_transientTextures.Count());
		TransientTextureDesc& srcReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		srcReadDesc.m_format = m_desc.srcFormat;
		srcReadDesc.m_width = m_desc.srcRect.x + m_desc.srcRect.w;
		srcReadDesc.m_height = m_desc.srcRect.y + m_desc.srcRect.h;
		srcReadDesc.m_name = m_desc.srcTextureName;
		srcReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		srcReadDesc.m_usageFlags = srcReadDesc.m_initialUsage;

		m_outputName = m_desc.srcTextureName;
		m_outputName += "_mergedOutput";

		AttachmentDesc& targetDesc = nodeDesc.m_attachments.PushBackInit();
		targetDesc.m_format = m_desc.srcFormat;
		targetDesc.m_width = nodeDesc.m_renderWidth;
		targetDesc.m_height = nodeDesc.m_renderHeight;
		targetDesc.m_name = m_outputName.c_str();
		targetDesc.m_externalTexture = m_desc.dstTexture;
		targetDesc.m_usage = PB::EAttachmentUsage::COLOR;
		targetDesc.m_flags = EAttachmentFlags::NONE;

		builder->AddNode(nodeDesc);
	}

	void MergeRenderPlanesPass::SetupPipeline(PB::RenderPass renderPass)
	{
		enum class EMergeRenderPlanesPermutation
		{
			SHADER_STAGE,
			CHANNEL_COUNT,
			PERMUTATION_END
		};

		PB::GraphicsPipelineDesc mergePipelineDesc{};

		AssetEncoder::ShaderPermutationTable<EMergeRenderPlanesPermutation> permTable{};
		permTable.SetPermutation(EMergeRenderPlanesPermutation::SHADER_STAGE, AssetEncoder::EShaderStagePermutation::VERTEX);
		mergePipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/mergeRenderPlanes", permTable.GetKey(), m_allocator, true).GetModule();

		permTable.SetPermutation(EMergeRenderPlanesPermutation::SHADER_STAGE, AssetEncoder::EShaderStagePermutation::FRAGMENT);
		permTable.SetPermutation(EMergeRenderPlanesPermutation::CHANNEL_COUNT, uint8_t(m_desc.srcChannelCount - 1));
		mergePipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/mergeRenderPlanes", permTable.GetKey(), m_allocator, true).GetModule();

		mergePipelineDesc.m_attachmentCount = 1;
		mergePipelineDesc.m_cullMode = PB::EFaceCullMode::BACK;
		mergePipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
		mergePipelineDesc.m_depthWriteEnable = false;
		mergePipelineDesc.m_stencilTestEnable = false;
		mergePipelineDesc.m_renderArea = { 0, 0, 0, 0 };
		mergePipelineDesc.m_renderPass = renderPass;

		auto& blendState = mergePipelineDesc.m_colorBlendStates[0];
		blendState = PB::GraphicsPipelineDesc::DefaultBlendState();

		m_pipeline = m_renderer->GetPipelineCache()->GetPipeline(mergePipelineDesc);
	}
};