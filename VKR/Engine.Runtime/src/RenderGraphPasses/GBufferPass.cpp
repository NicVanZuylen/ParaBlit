#include "GBufferPass.h"
#include "RenderGraph/RenderGraph.h"
#include "WorldRender/BatchDispatcher.h"
#include "WorldRender/RenderBoundingVolumeHierarchy.h"
#include "WorldRender/Camera.h"
#include "WorldRender/DrawBatch.h"

namespace Eng
{
	GBufferPass::GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator, PB::UniformBufferView viewConstView, PB::UniformBufferView viewPlanesView, const Camera* camera, RenderBoundingVolumeHierarchy* hierarchy) : RenderGraphBehaviour(renderer, allocator)
	{
		m_renderer = renderer;
		m_allocator = allocator;

		m_viewConstView = viewConstView;
		m_viewPlanesView = viewPlanesView;
		m_camera = camera;
		m_rbvh = hierarchy;

		m_batchDispatcher = m_allocator->Alloc<BatchDispatcher>(m_renderer, m_allocator);
		m_batchBindings.m_uniformBufferCount = 1;
		m_batchBindings.m_uniformBuffers = &m_viewConstView;
		m_batchBindings.m_resourceCount = 0;
		m_batchBindings.m_resourceViews = nullptr;
	}

	GBufferPass::~GBufferPass()
	{
		m_allocator->Free(m_batchDispatcher);
	}

	void GBufferPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (m_drawbatchPipeline == 0)
		{
			bool supportMeshShaders = m_renderer->GetDeviceLimitations()->m_supportMeshShader;

			PB::GraphicsPipelineDesc pipelineDesc = GetBasePipelineDesc();
			if (supportMeshShaders)
			{
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::TASK] = Eng::Shader(m_renderer, "Shaders/GLSL/ts_obj_meshlet_cull", m_allocator, true).GetModule();
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::MESH] = Eng::Shader(m_renderer, "Shaders/GLSL/ms_obj_task_batch", m_allocator, true).GetModule();
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_obj_def_mesh_batch", m_allocator, true).GetModule();
			}
			else
			{
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/vs_obj_def_batch", m_allocator, true).GetModule();
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_obj_def_batch", m_allocator, true).GetModule();
			}

			m_drawbatchPipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
		}

		m_rbvh->CullBatches(m_camera->GetFrustrum(), m_batchDispatcher, m_batchBindings);
		m_batchDispatcher->DispatchFrustrumCull(info.m_commandContext, m_viewPlanesView, true);
	}

	void GBufferPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		auto renderWidth = m_renderer->GetSwapchain()->GetWidth();
		auto renderHeight = m_renderer->GetSwapchain()->GetHeight();

		info.m_commandContext->CmdSetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
		info.m_commandContext->CmdSetScissor({ 0, 0, renderWidth, renderHeight });

		m_batchDispatcher->DrawBatches(info.m_commandContext, m_viewPlanesView, m_drawbatchPipeline, true);
	}

	void GBufferPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (m_outputTexture)
		{
			info.m_commandContext->CmdTransitionTexture(transientTextures[0], PB::ETextureState::COLORTARGET, PB::ETextureState::COPY_SRC);
			info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::NONE, PB::ETextureState::COPY_DST);

			info.m_commandContext->CmdCopyTextureToTexture(transientTextures[0], m_outputTexture);

			info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
		}
	}

	void GBufferPass::AddToRenderGraph(RenderGraphBuilder* builder)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = false;

		PB::ISwapChain* swapchain = m_renderer->GetSwapchain();

		AttachmentDesc& colorDesc = nodeDesc.m_attachments.PushBackInit();
		colorDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		colorDesc.m_width = swapchain->GetWidth();
		colorDesc.m_height = swapchain->GetHeight();
		colorDesc.m_name = "G_Color";
		colorDesc.m_usage = PB::EAttachmentUsage::COLOR;
		colorDesc.m_clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		colorDesc.m_flags = EAttachmentFlags::CLEAR;

		AttachmentDesc& normalDesc = nodeDesc.m_attachments.PushBackInit();
		normalDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
		normalDesc.m_width = swapchain->GetWidth();
		normalDesc.m_height = swapchain->GetHeight();
		normalDesc.m_name = "G_Normal";
		normalDesc.m_usage = PB::EAttachmentUsage::COLOR;
		normalDesc.m_clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		normalDesc.m_flags = EAttachmentFlags::CLEAR;

		AttachmentDesc& specAndRoughDesc = nodeDesc.m_attachments.PushBackInit();
		specAndRoughDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		specAndRoughDesc.m_width = swapchain->GetWidth();
		specAndRoughDesc.m_height = swapchain->GetHeight();
		specAndRoughDesc.m_name = "G_SpecAndRough";
		specAndRoughDesc.m_usage = PB::EAttachmentUsage::COLOR;

		AttachmentDesc& depthDesc = nodeDesc.m_attachments.PushBackInit();
		depthDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
		depthDesc.m_width = swapchain->GetWidth();
		depthDesc.m_height = swapchain->GetHeight();
		depthDesc.m_name = "G_Depth";
		depthDesc.m_usage = PB::EAttachmentUsage::DEPTHSTENCIL;
		depthDesc.m_clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		depthDesc.m_flags = EAttachmentFlags::CLEAR;

		nodeDesc.m_renderWidth = colorDesc.m_width;
		nodeDesc.m_renderHeight = colorDesc.m_height;

		builder->AddNode(nodeDesc);
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
		pipelineDesc.m_attachmentCount = 3;

		return pipelineDesc;
	}
};