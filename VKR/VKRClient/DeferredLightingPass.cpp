#include "DeferredLightingPass.h"

#include <random>

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
	lightViewDesc.m_size = sizeof(LightingBuffer::m_pointLights);
	m_pointLightingView = m_lightingBuffer->GetViewAsUniformBuffer(lightViewDesc);

	lightViewDesc.m_offset = sizeof(LightingBuffer::m_pointLights);
	lightViewDesc.m_size = sizeof(LightingBuffer::m_directionalLights);
	m_dirLightingView = m_lightingBuffer->GetViewAsUniformBuffer(lightViewDesc);

	PB::SamplerDesc gBufferSamplerDesc;
	gBufferSamplerDesc.m_anisotropyLevels = 1.0f;
	gBufferSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
	gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
	m_gBufferSampler = renderer->GetSampler(gBufferSamplerDesc);

	m_pointLightVolumeMesh.Init(m_renderer, "TestAssets/Primitives/sphere.obj");

	m_screenQuadShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/vs_screenQuad.spv", m_allocator);
	m_defDirLightShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/fs_def_directional_light_shadow.spv", m_allocator);
	m_pointLightVTXShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/vs_obj_point_light.spv", m_allocator);
	m_pointLightShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/fs_def_point_light.spv", m_allocator);

	PB::BufferObjectDesc indirectParamsDesc;
	indirectParamsDesc.m_bufferSize = sizeof(PB::DrawIndexedIndirectParams);
	indirectParamsDesc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
	indirectParamsDesc.m_usage = PB::EBufferUsage::INDIRECT_PARAMS;
	m_pointLightIndirectParamsBuffer = m_renderer->AllocateBuffer(indirectParamsDesc);
}

DeferredLightingPass::~DeferredLightingPass()
{
	if (m_reusableCmdList)
		m_renderer->FreeCommandList(m_reusableCmdList);

	m_renderer->FreeBuffer(m_lightingBuffer);

	m_allocator->Free(m_screenQuadShader);
	m_allocator->Free(m_defDirLightShader);
	m_allocator->Free(m_pointLightVTXShader);
	m_allocator->Free(m_pointLightShader);

	m_renderer->FreeBuffer(m_pointLightIndirectParamsBuffer);
}

void DeferredLightingPass::OnPrePass(const RenderGraphInfo& info)
{

}

void DeferredLightingPass::OnPassBegin(const RenderGraphInfo& info)
{
	if (m_mappedLightingBuffer)
	{
		m_lightingBuffer->EndPopulate();
		m_mappedLightingBuffer = nullptr;

		PB::u8* mappedPtr = m_pointLightIndirectParamsBuffer->BeginPopulate();
		PB::DrawIndexedIndirectParams indirectParams{};
		indirectParams.offset = 0;
		indirectParams.firstIndex = 0;
		indirectParams.indexCount = m_pointLightVolumeMesh.IndexCount();
		indirectParams.instanceCount = m_pointLightCount;
		indirectParams.vertexOffset = 0;
		m_pointLightIndirectParamsBuffer->PopulateWithDrawIndexedIndirectParams(mappedPtr, indirectParams);
		m_pointLightIndirectParamsBuffer->EndPopulate();
	}

	auto RecordPass = [&]()
	{
		PB::CommandContextDesc scopedContextDesc{};
		scopedContextDesc.m_renderer = m_renderer;
		scopedContextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;
		scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(scopedContextDesc);
		scopedContext->Begin(info.m_renderPass, info.m_frameBuffer);

		auto renderWidth = info.m_renderer->GetSwapchain()->GetWidth();
		auto renderHeight = info.m_renderer->GetSwapchain()->GetHeight();

		PB::UniformBufferView dirBufferViews[] = { m_mvpBuffer->GetViewAsUniformBuffer(), m_dirLightingView };
		PB::ResourceView dirResourceViews[]
		{
			static_cast<PB::ResourceView>(info.m_renderTargetViews[0]),
			static_cast<PB::ResourceView>(info.m_renderTargetViews[1]),
			static_cast<PB::ResourceView>(info.m_renderTargetViews[2]),
			static_cast<PB::ResourceView>(info.m_renderTargetViews[3]),
			static_cast<PB::ResourceView>(info.m_renderTargetViews[4]),
			m_gBufferSampler,
		};

		PB::BindingLayout dirBindingLayout{};
		dirBindingLayout.m_uniformBufferCount = _countof(dirBufferViews);
		dirBindingLayout.m_uniformBuffers = dirBufferViews;
		dirBindingLayout.m_resourceCount = _countof(dirResourceViews);
		dirBindingLayout.m_resourceViews = dirResourceViews;

		if (m_dirLightingPipeline == 0)
		{
			PB::GraphicsPipelineDesc lightingPipelineDesc{};
			lightingPipelineDesc.m_attachmentCount = 1;
			lightingPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS; // Always should disable depth testing.
			lightingPipelineDesc.m_renderArea = { 0, 0, renderWidth, renderHeight };
			lightingPipelineDesc.m_stencilTestEnable = false;
			lightingPipelineDesc.m_cullMode = PB::EFaceCullMode::FRONT;
			lightingPipelineDesc.m_subpass = 0;
			lightingPipelineDesc.m_renderPass = info.m_renderPass;
			lightingPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_screenQuadShader->GetModule();
			lightingPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_defDirLightShader->GetModule();

			m_dirLightingPipeline = m_renderer->GetPipelineCache()->GetPipeline(lightingPipelineDesc);
		}

		// Directional lighting
		{
			scopedContext->CmdBindPipeline(m_dirLightingPipeline);
			scopedContext->CmdBindResources(dirBindingLayout);
			scopedContext->CmdDraw(6, 1);
		}

		if (m_pointLightingPipeline == 0)
		{
			PB::GraphicsPipelineDesc lightingPipelineDesc{};
			lightingPipelineDesc.m_attachmentCount = 1;
			lightingPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS; // Always should disable depth testing.
			lightingPipelineDesc.m_renderArea = { 0, 0, renderWidth, renderHeight };
			lightingPipelineDesc.m_stencilTestEnable = false;
			lightingPipelineDesc.m_cullMode = PB::EFaceCullMode::FRONT;
			lightingPipelineDesc.m_subpass = 0;
			lightingPipelineDesc.m_renderPass = info.m_renderPass;
			lightingPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_pointLightVTXShader->GetModule();
			lightingPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_pointLightShader->GetModule();
			lightingPipelineDesc.m_vertexBuffers[0] = { sizeof(Vertex), PB::EVertexBufferType::VERTEX };
			lightingPipelineDesc.m_vertexDesc.vertexAttributes[0] = { 0, PB::EVertexAttributeType::FLOAT4 };
			lightingPipelineDesc.m_colorBlendStates[0] = PB::GraphicsPipelineDesc::DefaultBlendState();

			m_pointLightingPipeline = m_renderer->GetPipelineCache()->GetPipeline(lightingPipelineDesc);
		}

		// Point lighting
		if (m_pointLightCount > 0)
		{
			PB::UniformBufferView pntBufferViews[] = { m_mvpBuffer->GetViewAsUniformBuffer(), m_pointLightingView };
			PB::ResourceView pntResourceViews[]
			{
				static_cast<PB::ResourceView>(info.m_renderTargetViews[0]),
				static_cast<PB::ResourceView>(info.m_renderTargetViews[1]),
				static_cast<PB::ResourceView>(info.m_renderTargetViews[2]),
				static_cast<PB::ResourceView>(info.m_renderTargetViews[3]),
				m_gBufferSampler,
			};

			PB::BindingLayout pntBindingLayout{};
			pntBindingLayout.m_uniformBufferCount = _countof(pntBufferViews);
			pntBindingLayout.m_uniformBuffers = pntBufferViews;
			pntBindingLayout.m_resourceCount = _countof(pntResourceViews);
			pntBindingLayout.m_resourceViews = pntResourceViews;

			scopedContext->CmdBindPipeline(m_pointLightingPipeline);
			scopedContext->CmdBindResources(pntBindingLayout);
			scopedContext->CmdBindVertexBuffer(m_pointLightVolumeMesh.GetVertexBuffer(), m_pointLightVolumeMesh.GetIndexBuffer(), PB::EIndexType::PB_INDEX_TYPE_UINT32);
			scopedContext->CmdDrawIndexedIndirect(m_pointLightIndirectParamsBuffer, 0);
		}

		scopedContext->End();
		return scopedContext->Return();
	};
	if (!m_reusableCmdList)
		m_reusableCmdList = RecordPass();

	info.m_commandContext->CmdExecuteList(m_reusableCmdList);
}

void DeferredLightingPass::OnPostPass(const RenderGraphInfo& info)
{
	constexpr uint32_t outputTarget = 5;

	if (!m_outputTexture)
		return;

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

void DeferredLightingPass::SetDirectionalLight(uint32_t index, PB::Float4 direction, PB::Float4 color)
{
	if (!m_mappedLightingBuffer)
		m_mappedLightingBuffer = reinterpret_cast<LightingBuffer*>(m_lightingBuffer->BeginPopulate());

	auto& lights = m_mappedLightingBuffer->m_directionalLights;
	lights.m_lights[index].m_direction = direction;
	lights.m_lights[index].m_color = color;

	if(lights.m_lightCount <= index)
		m_mappedLightingBuffer->m_directionalLights.m_lightCount = index + 1;
}

void DeferredLightingPass::SetPointLight(uint32_t index, PB::Float4 position, PB::Float3 color, float radius)
{
	if (!m_mappedLightingBuffer)
		m_mappedLightingBuffer = reinterpret_cast<LightingBuffer*>(m_lightingBuffer->BeginPopulate());

	auto& light = m_mappedLightingBuffer->m_pointLights.m_lights[index];
	light.m_position = position;
	light.m_color = color;
	light.m_radius = radius;

	if (m_pointLightCount <= index)
		m_pointLightCount = index + 1;
}
