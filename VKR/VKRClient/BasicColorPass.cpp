#include "BasicColorPass.h"

using namespace PBClient;

BasicColorPass::BasicColorPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
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

	m_vertShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/vs_triangle.spv", m_allocator);
	m_fragShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/fs_triangle.spv", m_allocator);
}

BasicColorPass::~BasicColorPass()
{
	m_allocator->Free(m_paintTexture);
	m_allocator->Free(m_detailsTexture);
	m_allocator->Free(m_glassTexture);

	m_allocator->Free(m_paintMesh);
	m_allocator->Free(m_detailsMesh);
	m_allocator->Free(m_glassMesh);
}

void BasicColorPass::OnPreRenderPass(const RenderGraphInfo& info)
{

}

void BasicColorPass::OnPassBegin(const RenderGraphInfo& info)
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
	pipelineDesc.m_shaderModules[PB::EShaderStage::PB_SHADER_STAGE_VERTEX] = m_vertShader->GetModule();
	pipelineDesc.m_shaderModules[PB::EShaderStage::PB_SHADER_STAGE_FRAGMENT] = m_fragShader->GetModule();
	pipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL;

	auto& vertexDesc = pipelineDesc.m_vertexDesc;
	vertexDesc.vertexSize = sizeof(Vertex);
	vertexDesc.vertexAttributes[0] = PB::EVertexAttributeType::FLOAT4;
	vertexDesc.vertexAttributes[1] = PB::EVertexAttributeType::FLOAT4;
	vertexDesc.vertexAttributes[2] = PB::EVertexAttributeType::FLOAT4;
	vertexDesc.vertexAttributes[3] = PB::EVertexAttributeType::FLOAT2;
	auto pipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);

	cmdContext->CmdBindPipeline(pipeline);
	
	cmdContext->CmdBindResources(bindings);
	cmdContext->CmdBindVertexBuffer(m_paintMesh->GetVertexBuffer(), m_paintMesh->GetIndexBuffer(), PB::PB_INDEX_TYPE_UINT32);
	cmdContext->CmdDrawIndexed(m_paintMesh->IndexCount(), 1);
	
	bindings.m_textures = &m_detailsView;
	cmdContext->CmdBindResources(bindings);
	cmdContext->CmdBindVertexBuffer(m_detailsMesh->GetVertexBuffer(), m_detailsMesh->GetIndexBuffer(), PB::PB_INDEX_TYPE_UINT32);
	cmdContext->CmdDrawIndexed(m_detailsMesh->IndexCount(), 1);
	
	bindings.m_textures = &m_glassView;
	cmdContext->CmdBindResources(bindings);
	cmdContext->CmdBindVertexBuffer(m_glassMesh->GetVertexBuffer(), m_glassMesh->GetIndexBuffer(), PB::PB_INDEX_TYPE_UINT32);
	cmdContext->CmdDrawIndexed(m_glassMesh->IndexCount(), 1);
	
}

void BasicColorPass::OnPostRenderPass(const RenderGraphInfo& info)
{
	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[0], PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST);

	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(info.m_renderTargets[0], m_outputTexture);

	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT);
}

void BasicColorPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

void BasicColorPass::SetMVPBuffer(PB::IBufferObject* buffer)
{
	m_mvpBuffer = buffer;
}
