#include "GBufferPass.h"
#include "RenderGraph.h"

using namespace PBClient;

GBufferPass::GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
}

GBufferPass::~GBufferPass()
{
}

void GBufferPass::OnPrePass(const RenderGraphInfo& info)
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

void GBufferPass::OnPostPass(const RenderGraphInfo& info)
{

}

void GBufferPass::AddToRenderGraph(RenderGraphBuilder* builder)
{
	NodeDesc nodeDesc{};
	nodeDesc.m_behaviour = this;
	nodeDesc.m_useReusableCommandLists = true;

	PB::ISwapChain* swapchain = m_renderer->GetSwapchain();

	AttachmentDesc& colorDesc = nodeDesc.m_attachments[0];
	colorDesc.m_format = swapchain->GetImageFormat();
	colorDesc.m_width = swapchain->GetWidth();
	colorDesc.m_height = swapchain->GetHeight();
	colorDesc.m_name = "G_Color";
	colorDesc.m_usage = PB::EAttachmentUsage::COLOR;

	AttachmentDesc& normalDesc = nodeDesc.m_attachments[1];
	normalDesc.m_format = PB::ETextureFormat::R32G32B32A32_FLOAT;
	normalDesc.m_width = swapchain->GetWidth();
	normalDesc.m_height = swapchain->GetHeight();
	normalDesc.m_name = "G_Normal";
	normalDesc.m_usage = PB::EAttachmentUsage::COLOR;
	normalDesc.m_clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	normalDesc.m_flags = EAttachmentFlags::CLEAR;

	AttachmentDesc& specAndRoughDesc = nodeDesc.m_attachments[2];
	specAndRoughDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
	specAndRoughDesc.m_width = swapchain->GetWidth();
	specAndRoughDesc.m_height = swapchain->GetHeight();
	specAndRoughDesc.m_name = "G_SpecAndRough";
	specAndRoughDesc.m_usage = PB::EAttachmentUsage::COLOR;

	AttachmentDesc& emissionDesc = nodeDesc.m_attachments[3];
	emissionDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
	emissionDesc.m_width = swapchain->GetWidth();
	emissionDesc.m_height = swapchain->GetHeight();
	emissionDesc.m_name = "G_Emission";
	emissionDesc.m_usage = PB::EAttachmentUsage::COLOR;
	emissionDesc.m_clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	emissionDesc.m_flags = EAttachmentFlags::CLEAR;

	AttachmentDesc& depthDesc = nodeDesc.m_attachments[4];
	depthDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
	depthDesc.m_width = swapchain->GetWidth();
	depthDesc.m_height = swapchain->GetHeight();
	depthDesc.m_name = "G_Depth";
	depthDesc.m_usage = PB::EAttachmentUsage::DEPTHSTENCIL;
	depthDesc.m_clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	depthDesc.m_flags = EAttachmentFlags::CLEAR;

	nodeDesc.m_attachmentCount = 5;
	nodeDesc.m_renderWidth = colorDesc.m_width;
	nodeDesc.m_renderHeight = colorDesc.m_height;

	builder->AddNode(nodeDesc);
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

PB::GraphicsPipelineDesc GBufferPass::GetBasePipelineDesc() const
{
	assert(m_renderPass && "Cannot get optimal pipeline desc before adding this node to a RenderGraph.");

	PB::GraphicsPipelineDesc pipelineDesc{};
	pipelineDesc.m_renderPass = m_renderPass;
	pipelineDesc.m_subpass = 0;
	pipelineDesc.m_renderArea = { 0, 0, 0, 0 };
	pipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL;
	pipelineDesc.m_attachmentCount = 4;

	return pipelineDesc;
}
