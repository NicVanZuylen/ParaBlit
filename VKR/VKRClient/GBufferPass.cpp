#include "GBufferPass.h"

using namespace PBClient;

GBufferPass::GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;

	m_paintMesh = m_allocator->Alloc<Mesh>(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_paint.obj");
	m_detailsMesh = m_allocator->Alloc<Mesh>(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_details.obj");
	m_glassMesh = m_allocator->Alloc<Mesh>(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_glass.obj");
	
	m_paintTexture = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_diffuse.tga");
	m_detailsTexture = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_diffuse.tga");
	m_glassTexture = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_diffuse.tga");

	m_paintView = m_paintTexture->GetTexture()->GetDefaultSRV();
	m_detailsView = m_detailsTexture->GetTexture()->GetDefaultSRV();
	m_glassView = m_glassTexture->GetTexture()->GetDefaultSRV();

	m_vertShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/vs_obj_def.spv", m_allocator);
	m_fragShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/fs_obj_def.spv", m_allocator);

	PB::SamplerDesc colorSamplerDesc;
	colorSamplerDesc.m_anisotropyLevels = 1.0f;
	colorSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	colorSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
	m_colorSampler = m_renderer->GetSampler(colorSamplerDesc);
}

GBufferPass::~GBufferPass()
{
	m_allocator->Free(m_paintTexture);
	m_allocator->Free(m_detailsTexture);
	m_allocator->Free(m_glassTexture);

	m_allocator->Free(m_paintMesh);
	m_allocator->Free(m_detailsMesh);
	m_allocator->Free(m_glassMesh);

	m_allocator->Free(m_vertShader);
	m_allocator->Free(m_fragShader);
}

void GBufferPass::OnPreRenderPass(const RenderGraphInfo& info)
{

}

void GBufferPass::OnPassBegin(const RenderGraphInfo& info)
{
	PB::BufferViewDesc mvpViewDesc;
	mvpViewDesc.m_buffer = m_mvpBuffer;
	mvpViewDesc.m_offset = 0;
	mvpViewDesc.m_size = m_mvpBuffer->GetSize();
	PB::BufferView mvpView = m_mvpBuffer->GetView();

	PB::BindingLayout bindings{};
	bindings.m_bindingLocation = PB::EBindingLayoutLocation::DEFAULT;
	bindings.m_bufferCount = 1;
	bindings.m_buffers = &mvpView;
	bindings.m_textureCount = 1;
	bindings.m_textures = &m_paintView;
	bindings.m_samplerCount = 1;
	bindings.m_samplers = &m_colorSampler;

	auto cmdContext = info.m_commandContext;

	PB::PipelineDesc pipelineDesc{};
	pipelineDesc.m_renderPass = info.m_renderPass;
	pipelineDesc.m_subpass = 0;
	pipelineDesc.m_renderArea = { 0, 0, info.m_renderer->GetSwapchain()->GetWidth(), info.m_renderer->GetSwapchain()->GetHeight() };
	pipelineDesc.m_shaderModules[PB::EShaderStage::VERTEX] = m_vertShader->GetModule();
	pipelineDesc.m_shaderModules[PB::EShaderStage::FRAGMENT] = m_fragShader->GetModule();
	pipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL;
	pipelineDesc.m_attachmentCount = 2;
	pipelineDesc.m_vertexBuffers[0] = { sizeof(Vertex), PB::EVertexBufferType::VERTEX };
	pipelineDesc.m_vertexBuffers[1] = { sizeof(Vertex), PB::EVertexBufferType::INSTANCE };

	auto& vertexDesc = pipelineDesc.m_vertexDesc;
	vertexDesc.vertexAttributes[0] = { 0, PB::EVertexAttributeType::FLOAT4 };
	vertexDesc.vertexAttributes[1] = { 0, PB::EVertexAttributeType::FLOAT4 };
	vertexDesc.vertexAttributes[2] = { 0, PB::EVertexAttributeType::FLOAT4 };
	vertexDesc.vertexAttributes[3] = { 0, PB::EVertexAttributeType::FLOAT2 };
	vertexDesc.vertexAttributes[4] = { 1, PB::EVertexAttributeType::MAT4 };
	auto pipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);

	cmdContext->CmdBindPipeline(pipeline);	
	cmdContext->CmdBindResources(bindings);

	const PB::IBufferObject* vertexBuffers[2] = { m_paintMesh->GetVertexBuffer(), m_instanceBuffer };
	constexpr const PB::u32 vertexBufferCount = _countof(vertexBuffers);
	cmdContext->CmdBindVertexBuffers(vertexBuffers, vertexBufferCount, m_paintMesh->GetIndexBuffer(), PB::EIndexType::PB_INDEX_TYPE_UINT32);
	cmdContext->CmdDrawIndexed(m_paintMesh->IndexCount(), 1);
	
	bindings.m_textures = &m_detailsView;
	cmdContext->CmdBindResources(bindings);
	vertexBuffers[0] = m_detailsMesh->GetVertexBuffer();
	cmdContext->CmdBindVertexBuffers(vertexBuffers, vertexBufferCount, m_detailsMesh->GetIndexBuffer(), PB::EIndexType::PB_INDEX_TYPE_UINT32);
	cmdContext->CmdDrawIndexed(m_detailsMesh->IndexCount(), 1);
	
	bindings.m_textures = &m_glassView;
	cmdContext->CmdBindResources(bindings);
	vertexBuffers[0] = m_glassMesh->GetVertexBuffer();
	cmdContext->CmdBindVertexBuffers(vertexBuffers, vertexBufferCount, m_glassMesh->GetIndexBuffer(), PB::EIndexType::PB_INDEX_TYPE_UINT32);
	cmdContext->CmdDrawIndexed(m_glassMesh->IndexCount(), 1);
	
}

void GBufferPass::OnPostRenderPass(const RenderGraphInfo& info)
{
	if (!m_outputTexture)
		return;

	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[0], PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST);
	
	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(info.m_renderTargets[0], m_outputTexture);
	
	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT);
}

void GBufferPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

void GBufferPass::SetMVPBuffer(PB::IBufferObject* buffer)
{
	m_mvpBuffer = buffer;
}

void GBufferPass::SetInstanceBuffer(PB::IBufferObject* buffer)
{
	m_instanceBuffer = buffer;
}
