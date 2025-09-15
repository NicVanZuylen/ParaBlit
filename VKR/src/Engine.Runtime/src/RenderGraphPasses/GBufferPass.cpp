#include "GBufferPass.h"
#include "RenderGraph/RenderGraph.h"
#include "WorldRender/BatchDispatcher.h"
#include "WorldRender/RenderBoundingVolumeHierarchy.h"
#include "WorldRender/DynamicDrawPool.h"
#include "Entity/Component/Camera.h"
#include "WorldRender/DrawBatch.h"
#include "Entity/EntityHierarchy.h"

namespace Eng
{
	GBufferPass::GBufferPass
	(
		PB::IRenderer* renderer,
		CLib::Allocator* allocator,
		PB::UniformBufferView viewConstView,
		PB::UniformBufferView viewPlanesView,
		const Camera* camera,
		EntityHierarchy* hierarchyToDraw
	) : RenderGraphBehaviour(renderer, allocator)
	{
		m_renderer = renderer;
		m_allocator = allocator;

		m_viewConstView = viewConstView;
		m_viewPlanesView = viewPlanesView;
		m_camera = camera;
		m_hierarchyToDraw = hierarchyToDraw;

		m_batchDispatcher = m_allocator->Alloc<BatchDispatcher>(m_renderer, m_allocator);
		m_batchBindings.m_uniformBufferCount = 1;
		m_batchBindings.m_uniformBuffers = &m_viewConstView;
		m_batchBindings.m_resourceCount = 0;
		m_batchBindings.m_resourceViews = nullptr;

		m_useMeshShaders = m_renderer->GetDeviceLimitations()->m_supportMeshShader;
	}

	GBufferPass::~GBufferPass()
	{
		m_allocator->Free(m_batchDispatcher);
	}

	void GBufferPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("GBufferPass::OnPrePass", { 1.0f, 1.0f, 1.0f, 1.0f });

		if (m_drawbatchPipeline == 0)
		{
			PB::GraphicsPipelineDesc pipelineDesc = GetBasePipelineDesc();
			if (m_useMeshShaders)
			{
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::TASK] = Eng::Shader(m_renderer, "Shaders/GLSL/ts_obj_meshlet_cull", 0, m_allocator, true).GetModule();
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::MESH] = Eng::Shader(m_renderer, "Shaders/GLSL/ms_obj_task_batch", 0, m_allocator, true).GetModule();
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_obj_def_mesh_batch", 0, m_allocator, true).GetModule();
			}
			else
			{
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/vs_obj_def_batch", 0, m_allocator, true).GetModule();
				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_obj_def_batch", 0, m_allocator, true).GetModule();
			}

			m_drawbatchPipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
		}

		m_batchDispatcher->DispatchFrustrumCull(info.m_commandContext, m_viewPlanesView, m_useMeshShaders);

		m_hierarchyToDraw->GetDynamicDrawPool().UpdateComputeGPU(info.m_commandContext, m_viewPlanesView);
		m_hierarchyToDraw->GetStaticObjectRenderer().UpdateComputeGPU(info.m_commandContext, m_viewPlanesView);

		info.m_commandContext->CmdEndLastLabel();
	}

	void GBufferPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("GBufferPass", { 1.0f, 1.0f, 1.0f, 1.0f });

		info.m_commandContext->CmdSetViewport({ 0, 0, m_targetResolution.x, m_targetResolution.y }, 0.0f, 1.0f);
		info.m_commandContext->CmdSetScissor({ 0, 0, m_targetResolution.x, m_targetResolution.y });

		m_batchDispatcher->DrawBatches(info.m_commandContext, m_viewPlanesView, m_drawbatchPipeline, m_useMeshShaders);
		
		info.m_commandContext->CmdBindPipeline(m_drawbatchPipeline);
		m_hierarchyToDraw->GetDynamicDrawPool().Draw(info.m_commandContext, m_viewConstView, m_viewPlanesView);
		m_hierarchyToDraw->GetStaticObjectRenderer().Draw(info.m_commandContext, m_viewConstView, m_viewPlanesView);

		info.m_commandContext->CmdEndLastLabel();
	}

	void GBufferPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("GBufferPass::OnPostPass", { 1.0f, 1.0f, 1.0f, 1.0f });

		if (m_outputTexture)
		{
			info.m_commandContext->CmdTransitionTexture(transientTextures[0], PB::ETextureState::COLORTARGET, PB::ETextureState::COPY_SRC);
			info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::NONE, PB::ETextureState::COPY_DST);

			info.m_commandContext->CmdCopyTextureToTexture(transientTextures[0], m_outputTexture);

			info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
		}

		info.m_commandContext->CmdEndLastLabel();
	}

	void GBufferPass::AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = false;

		AttachmentDesc& colorDesc = nodeDesc.m_attachments.PushBackInit();
		colorDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		colorDesc.m_width = targetResolution.x;
		colorDesc.m_height = targetResolution.y;
		colorDesc.m_name = "G_Color";
		colorDesc.m_usage = PB::EAttachmentUsage::COLOR;
		colorDesc.m_clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		colorDesc.m_flags = EAttachmentFlags::CLEAR;

		AttachmentDesc& normalDesc = nodeDesc.m_attachments.PushBackInit();
		normalDesc.m_format = PB::ETextureFormat::A2R10G10B10_UNORM;
		normalDesc.m_width = targetResolution.x;
		normalDesc.m_height = targetResolution.y;
		normalDesc.m_name = "G_Normal";
		normalDesc.m_usage = PB::EAttachmentUsage::COLOR;
		normalDesc.m_clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		normalDesc.m_flags = EAttachmentFlags::CLEAR;

		AttachmentDesc& specAndRoughDesc = nodeDesc.m_attachments.PushBackInit();
		specAndRoughDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		specAndRoughDesc.m_width = targetResolution.x;
		specAndRoughDesc.m_height = targetResolution.y;
		specAndRoughDesc.m_name = "G_SpecAndRough";
		specAndRoughDesc.m_usage = PB::EAttachmentUsage::COLOR;

		AttachmentDesc& motionVectorsDesc = nodeDesc.m_attachments.PushBackInit();
		motionVectorsDesc.m_format = PB::ETextureFormat::R16G16_FLOAT;
		motionVectorsDesc.m_width = targetResolution.x;
		motionVectorsDesc.m_height = targetResolution.y;
		motionVectorsDesc.m_name = "G_MotionVectors";
		motionVectorsDesc.m_usage = PB::EAttachmentUsage::COLOR;
		motionVectorsDesc.m_flags = EAttachmentFlags::CLEAR;

		AttachmentDesc& depthDesc = nodeDesc.m_attachments.PushBackInit();
		depthDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
		depthDesc.m_width = targetResolution.x;
		depthDesc.m_height = targetResolution.y;
		depthDesc.m_name = "G_Depth";
		depthDesc.m_usage = PB::EAttachmentUsage::DEPTHSTENCIL;
		depthDesc.m_clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		depthDesc.m_flags = EAttachmentFlags::CLEAR;

		nodeDesc.m_renderWidth = colorDesc.m_width;
		nodeDesc.m_renderHeight = colorDesc.m_height;
		m_targetResolution = targetResolution;

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
		pipelineDesc.m_attachmentCount = 4;
		pipelineDesc.m_cullMode = PB::EFaceCullMode::BACK;

		return pipelineDesc;
	}
};