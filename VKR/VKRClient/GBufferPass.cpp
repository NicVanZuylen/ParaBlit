#include "GBufferPass.h"

#include "DrawBatch.h"

using namespace PBClient;

GBufferPass::GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
}

GBufferPass::~GBufferPass()
{
}

void GBufferPass::OnPreRenderPass(const RenderGraphInfo& info)
{
	if (m_listRequiresUpdate)
	{
		m_geoDispatchList->Update(info.m_commandContext);
		m_listRequiresUpdate = false;
	}
}

void GBufferPass::OnPassBegin(const RenderGraphInfo& info)
{
	if (m_geoDispatchList)
		m_geoDispatchList->Dispatch(info.m_commandContext, info.m_renderPass, info.m_frameBuffer);
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

void GBufferPass::SetDispatchList(ObjectDispatchList* list, bool updateList)
{
	m_geoDispatchList = list;
	m_listRequiresUpdate |= updateList;
}

void GBufferPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

PB::GraphicsPipelineDesc GBufferPass::GetBasePipelineDesc(PB::ISwapChain* swapChain) const
{
	assert(m_renderPass && "Cannot get optimal pipeline desc before adding this node to a RenderGraph.");

	PB::GraphicsPipelineDesc pipelineDesc{};
	pipelineDesc.m_renderPass = m_renderPass;
	pipelineDesc.m_subpass = 0;
	pipelineDesc.m_renderArea = { 0, 0, swapChain->GetWidth(), swapChain->GetHeight() };
	pipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL;
	pipelineDesc.m_attachmentCount = 3;

	return pipelineDesc;
}
