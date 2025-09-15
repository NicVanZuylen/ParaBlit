#include "ImGUIRenderPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Engine.ParaBlit/IImGUIModule.h"

#include <Engine.Math/Vectors.h>
#include <Engine.Math/Matrix4.h>

namespace Eng
{
	ImGUIRenderPass::ImGUIRenderPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
	{
		m_renderer = renderer;
		m_allocator = allocator;

		
	}

	ImGUIRenderPass::~ImGUIRenderPass()
	{
		
	}

	void ImGUIRenderPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		
	}

	void ImGUIRenderPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("ImGUIRenderPass", { 1.0f, 1.0f, 1.0f, 1.0f });

		auto renderWidth = m_renderer->GetSwapchain()->GetWidth();
		auto renderHeight = m_renderer->GetSwapchain()->GetHeight();

		m_renderer->GetImGUIModule()->RenderDrawData(m_drawData, info.m_commandContext);

		info.m_commandContext->CmdEndLastLabel();
	}

	void ImGUIRenderPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (!m_outputTexture)
			return;

		// Transition color and output to correct layouts...
		info.m_commandContext->CmdTransitionTexture(transientTextures[0], PB::ETextureState::COLORTARGET, PB::ETextureState::COPY_SRC);
		info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT, PB::ETextureState::COPY_DST);

		// Copy color to output.
		info.m_commandContext->CmdCopyTextureToTexture(transientTextures[0], m_outputTexture);

		// Transition output texture to present.
		info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
	}

	void ImGUIRenderPass::AddToRenderGraph(RenderGraphBuilder* builder)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = false;
		nodeDesc.m_computeOnlyPass = false;
		nodeDesc.m_renderWidth = m_renderer->GetSwapchain()->GetWidth();
		nodeDesc.m_renderHeight = m_renderer->GetSwapchain()->GetHeight();

		AttachmentDesc& targetDesc = nodeDesc.m_attachments.PushBackInit();
		targetDesc.m_format = m_renderer->GetSwapchain()->GetImageFormat();
		targetDesc.m_width = nodeDesc.m_renderWidth;
		targetDesc.m_height = nodeDesc.m_renderHeight;
		targetDesc.m_name = "MergedOutput";
		targetDesc.m_usage = PB::EAttachmentUsage::COLOR;
		targetDesc.m_flags = EAttachmentFlags::CLEAR;

		TransientTextureDesc& targetReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		targetReadDesc.m_format = m_renderer->GetSwapchain()->GetImageFormat();
		targetReadDesc.m_width = nodeDesc.m_renderWidth;
		targetReadDesc.m_height = nodeDesc.m_renderHeight;
		targetReadDesc.m_name = "MergedOutput";
		targetReadDesc.m_initialUsage = PB::ETextureState::COLORTARGET;
		targetReadDesc.m_finalUsage = PB::ETextureState::COPY_SRC;
		targetReadDesc.m_usageFlags = PB::ETextureState::COLORTARGET | PB::ETextureState::COPY_SRC;

		builder->AddNode(nodeDesc);
	}

	void ImGUIRenderPass::SetOutputTexture(PB::ITexture* tex)
	{
		m_outputTexture = tex;
	}
};