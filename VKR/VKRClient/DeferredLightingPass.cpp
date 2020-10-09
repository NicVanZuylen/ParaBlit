#include "DeferredLightingPass.h"

using namespace PBClient;

DeferredLightingPass::DeferredLightingPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	PB::BufferObjectDesc lightingBufferDesc;
	lightingBufferDesc.m_bufferSize = sizeof(LightingBuffer);
	lightingBufferDesc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
	lightingBufferDesc.m_usage = PB::EBufferUsage::UNIFORM;
	m_lightingBuffer = renderer->AllocateBuffer(lightingBufferDesc);

	PB::BufferViewDesc lightViewDesc;
	lightViewDesc.m_offset = 0;
	lightViewDesc.m_size = sizeof(LightingBuffer::m_directionalLights);
	m_dirLightingView = m_lightingBuffer->GetView();

	lightViewDesc.m_offset = sizeof(LightingBuffer::m_directionalLights);
	lightViewDesc.m_size = sizeof(LightingBuffer::m_pointLights);
	m_pointLightView = m_lightingBuffer->GetView(lightViewDesc);

	PB::SamplerDesc gBufferSamplerDesc;
	gBufferSamplerDesc.m_anisotropyLevels = 1.0f;
	gBufferSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP;
	m_gBufferSampler = renderer->GetSampler(gBufferSamplerDesc);

	m_screenQuadShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/vs_screenQuad.spv", m_allocator);
	m_defDirLightShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/fs_def_directional_light.spv", m_allocator);

	{
		// Set up lighting
		LightingBuffer* lightingData = reinterpret_cast<LightingBuffer*>(m_lightingBuffer->BeginPopulate());

		lightingData->m_directionalLights.m_lights[0].m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
		lightingData->m_directionalLights.m_lights[0].m_direction = { 0.0f, 1.0f, 0.0f, 1.0f };
		lightingData->m_directionalLights.m_lightCount = 1;

		m_lightingBuffer->EndPopulate();
	}
}

DeferredLightingPass::~DeferredLightingPass()
{
	m_renderer->FreeBuffer(m_lightingBuffer);

	m_allocator->Free(m_screenQuadShader);
	m_allocator->Free(m_defDirLightShader);
}

void DeferredLightingPass::OnPreRenderPass(const RenderGraphInfo& info)
{
	
}

void DeferredLightingPass::OnPassBegin(const RenderGraphInfo& info)
{
	PB::BufferView bufferViews[2] = { m_mvpBuffer->GetView(), m_dirLightingView };

	PB::BindingLayout bindingLayout{};
	bindingLayout.m_bufferCount = _countof(bufferViews);
	bindingLayout.m_buffers = bufferViews;
	bindingLayout.m_samplerCount = 1;
	bindingLayout.m_samplers = &m_gBufferSampler;
	bindingLayout.m_textureCount = 3;
	bindingLayout.m_textures = info.m_renderTargetViews; // Views should already be in the correct order.

	auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth();
	auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight();

	if (m_dirLightingPipeline == 0)
	{
		PB::PipelineDesc lightingPipelineDesc{};
		lightingPipelineDesc.m_attachmentCount = 1;
		lightingPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS; // Always should disable depth testing.
		lightingPipelineDesc.m_renderArea = { 0, 0, renderWidth, renderHeight };
		lightingPipelineDesc.m_stencilTestEnable = false;
		lightingPipelineDesc.m_cullMode = PB::EFaceCullMode::FRONT;
		lightingPipelineDesc.m_subpass = 0;
		lightingPipelineDesc.m_renderPass = info.m_renderPass;
		lightingPipelineDesc.m_shaderModules[PB::EShaderStage::VERTEX] = m_screenQuadShader->GetModule();
		lightingPipelineDesc.m_shaderModules[PB::EShaderStage::FRAGMENT] = m_defDirLightShader->GetModule();

		m_dirLightingPipeline = m_renderer->GetPipelineCache()->GetPipeline(lightingPipelineDesc);
	}

	info.m_commandContext->CmdBindPipeline(m_dirLightingPipeline);
	
	info.m_commandContext->CmdBindResources(bindingLayout);
	
	// Draw directional lighting...
	info.m_commandContext->CmdDraw(6, 1);
}

void DeferredLightingPass::OnPostRenderPass(const RenderGraphInfo& info)
{
	constexpr uint32_t outputTarget = 3;

	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[outputTarget], PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST);

	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(info.m_renderTargets[outputTarget], m_outputTexture);

	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT);
}

void DeferredLightingPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

void DeferredLightingPass::SetMVPBuffer(PB::IBufferObject* buf)
{
	m_mvpBuffer = buf;
}
