#include "BloomBlurPass.h"
#include "Engine.ParaBlit/IPipelineCache.h"
#include "Engine.ParaBlit/ParaBlitDefs.h"
#include "Engine.ParaBlit/ParaBlitImplUtil.h"
#include "RenderGraph/RenderGraph.h"
#include "Resource/Shader.h"
#include "WorldRender/BlurHelper.h"

namespace Eng
{
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
		if (m_blurCmdList)
		{
			m_renderer->FreeCommandList(m_blurCmdList);
			m_blurCmdList = nullptr;
		}

		if (m_mergeCmdList)
		{
			m_renderer->FreeCommandList(m_mergeCmdList);
			m_mergeCmdList = nullptr;
		}
	}

	void BloomBlurPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("BloomBlurPass", { 1.0f, 1.0f, 1.0f, 1.0f });

		auto RecordPass = [&]()
		{
			PB::u32 halfResDenom = m_halfRes ? 2u : 1u;

			auto renderWidth = m_targetResolution.x / halfResDenom;
			auto renderHeight = m_targetResolution.y / halfResDenom;
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
				blurParams.m_imageFormat = PB::ETextureFormat::R8G8B8A8_UNORM;

				PB::Uint2 mipDimensions(renderWidth >> i, renderHeight >> i);
				m_blurHelper.Encode(scopedContext.GetContext(), mipDimensions, blurParams);

				PB::SubresourceRange subresources{};
				subresources.m_baseMip = i;
				subresources.m_mipCount = 1;

				scopedContext->CmdTransitionTexture(transientTextures[1], PB::ETextureState::STORAGE, PB::ETextureState::SAMPLED, subresources);
				scopedContext->CmdTransitionTexture(transientTextures[2], PB::ETextureState::SAMPLED, PB::ETextureState::STORAGE, subresources);
			}

			scopedContext->End();
			return scopedContext->Return();
		};		
		if (!m_blurCmdList)
			m_blurCmdList = RecordPass();

		info.m_commandContext->CmdExecuteList(m_blurCmdList);
		info.m_commandContext->CmdEndLastLabel();

		// Merge pass label...
		info.m_commandContext->CmdBeginLabel("BloomMergePass", { 1.0f, 1.0f, 1.0f, 1.0f });
	}

	void BloomBlurPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		auto RecordPass = [&]()
		{
			PB::CommandContextDesc scopedContextDesc{};
			scopedContextDesc.m_renderer = m_renderer;
			scopedContextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;
			scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

			PB::SCommandContext scopedContext(m_renderer);
			scopedContext->Init(scopedContextDesc);
			scopedContext->Begin(info.m_renderPass, info.m_frameBuffer);

			PB::Pipeline mergePipeline = 0;
			{
				PB::GraphicsPipelineDesc mergePipelineDesc{};
				mergePipelineDesc.m_renderPass = info.m_renderPass;
				mergePipelineDesc.m_subpass = 0;
				mergePipelineDesc.m_renderArea = { 0, 0, 0, 0 };
				mergePipelineDesc.m_depthWriteEnable = false;
				mergePipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
				mergePipelineDesc.m_attachmentCount = 1;
				mergePipelineDesc.m_cullMode = PB::EFaceCullMode::FRONT;
				mergePipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/vs_screenQuad", 0, m_allocator, true).GetModule();
				mergePipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_merge_bloom", 0, m_allocator, true).GetModule();

				mergePipeline = m_renderer->GetPipelineCache()->GetPipeline(mergePipelineDesc);
			}

			scopedContext->CmdBindPipeline(mergePipeline);
			scopedContext->CmdSetViewport({ 0, 0, m_targetResolution.x, m_targetResolution.y }, 0.0f, 1.0f);
			scopedContext->CmdSetScissor({ 0, 0, m_targetResolution.x, m_targetResolution.y });

			PB::TextureViewDesc mergeSrcView{};
			mergeSrcView.m_texture = transientTextures[1];
			mergeSrcView.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
			mergeSrcView.m_expectedState = PB::ETextureState::SAMPLED;
			mergeSrcView.m_subresources.m_mipCount = BlurTargetMipCount;

			PB::ResourceView mergeResources[]
			{
				transientTextures[0]->GetDefaultSRV(),
				transientTextures[1]->GetView(mergeSrcView),
				m_mergeSampler,
			};

			PB::BindingLayout mergeBindings;
			mergeBindings.m_uniformBufferCount = 0;
			mergeBindings.m_uniformBuffers = nullptr;
			mergeBindings.m_resourceCount = PB_ARRAY_LENGTH(mergeResources);
			mergeBindings.m_resourceViews = mergeResources;

			scopedContext->CmdBindResources(mergeBindings);
			scopedContext->CmdDraw(6, 1);

			scopedContext->End();
			return scopedContext->Return();
		};
		if (!m_mergeCmdList)
			m_mergeCmdList = RecordPass();

		info.m_commandContext->CmdExecuteList(m_mergeCmdList);
	}
	
	void BloomBlurPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdEndLastLabel(); // Merge pass label...

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

	void BloomBlurPass::AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution)
	{
		if (m_blurCmdList)
		{
			m_renderer->FreeCommandList(m_blurCmdList);
			m_blurCmdList = nullptr;
		}

		if (m_mergeCmdList)
		{
			m_renderer->FreeCommandList(m_mergeCmdList);
			m_mergeCmdList = nullptr;
		}

		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = true;
		nodeDesc.m_computeOnlyPass = false;

		PB::ISwapChain* swapchain = m_renderer->GetSwapchain();
		PB::u32 halfResDenom = m_halfRes ? 2u : 1u;
		PB::u32 renderWidth = targetResolution.x / halfResDenom;
		PB::u32 renderHeight = targetResolution.y / halfResDenom;

		TransientTextureDesc& colorDesc = nodeDesc.m_transientTextures.PushBackInit();
		colorDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
		colorDesc.m_width = targetResolution.x;
		colorDesc.m_height = targetResolution.y;
		colorDesc.m_name = "LightingColorOutput";
		colorDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		colorDesc.m_usageFlags = colorDesc.m_initialUsage;

		TransientTextureDesc& outColorDesc = nodeDesc.m_transientTextures.PushBackInit();
		outColorDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		outColorDesc.m_width = renderWidth;
		outColorDesc.m_height = renderHeight;
		outColorDesc.m_mipCount = BlurTargetMipCount;
		outColorDesc.m_name = "BloomColorOutput";
		outColorDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		outColorDesc.m_finalUsage = PB::ETextureState::STORAGE;
		outColorDesc.m_usageFlags = PB::ETextureState::SAMPLED | PB::ETextureState::STORAGE;

		TransientTextureDesc& blurColorBufferDesc = nodeDesc.m_transientTextures.PushBackInit();
		blurColorBufferDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		blurColorBufferDesc.m_width = renderWidth;
		blurColorBufferDesc.m_height = renderHeight;
		blurColorBufferDesc.m_mipCount = BlurTargetMipCount;
		blurColorBufferDesc.m_name = "BloomBlurColor";
		blurColorBufferDesc.m_initialUsage = PB::ETextureState::STORAGE;
		blurColorBufferDesc.m_usageFlags = PB::ETextureState::SAMPLED | PB::ETextureState::STORAGE;

		AttachmentDesc& mergedOutputDesc = nodeDesc.m_attachments.PushBackInit();
		mergedOutputDesc.m_format = PB::Util::FormatToUnorm(swapchain->GetImageFormat());
		mergedOutputDesc.m_width = targetResolution.x;
		mergedOutputDesc.m_height = targetResolution.y;
		mergedOutputDesc.m_name = "MergedOutput";
		mergedOutputDesc.m_usage = PB::EAttachmentUsage::COLOR;

		nodeDesc.m_renderWidth = targetResolution.x;
		nodeDesc.m_renderHeight = targetResolution.y;
		m_targetResolution = targetResolution;

		builder->AddNode(nodeDesc);
	}

	void BloomBlurPass::SetOutputTexture(PB::ITexture* tex)
	{
		m_outputTexture = tex;
	}
};