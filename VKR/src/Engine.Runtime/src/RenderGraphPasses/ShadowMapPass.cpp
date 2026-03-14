#include "ShadowMapPass.h"
#include "Camera.h"
#include "DebugLinePass.h"
#include "Engine.Math/Vector3.h"
#include "Engine.ParaBlit/ParaBlitDefs.h"
#include "RenderGraph/RenderGraph.h"
#include "WorldRender/RenderBoundingVolumeHierarchy.h"
#include "WorldRender/BatchDispatcher.h"
#include "Entity/EntityHierarchy.h"

namespace Eng
{
	ShadowMapPass::ShadowMapPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc) : RenderGraphBehaviour(renderer, allocator)
	{
		m_renderer = renderer;
		m_allocator = allocator;

		m_frustrumSectionNear = desc.m_frustrumSectionNear;
		m_frustrumSectionFar = desc.m_frustrumSectionFar;
		m_softShadowPenumbraDistance = desc.m_softShadowPenumbraDistance;
		m_shadowBiasMultiplier = desc.m_shadowBiasMultiplier;
		m_shadowmapResolution = desc.m_shadowmapResolution;
		m_cascadeIndex = desc.m_cascadeIndex;

		PB::BufferObjectDesc shadowViewDesc;
		shadowViewDesc.m_bufferSize = sizeof(ShadowConstants);
		shadowViewDesc.m_options = 0;
		shadowViewDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
		m_shadowViewBuffer = m_renderer->AllocateBuffer(shadowViewDesc);
		m_shadowConstantsView = m_shadowViewBuffer->GetViewAsUniformBuffer();

		PB::BufferObjectDesc shadowPlanesViewDesc;
		shadowPlanesViewDesc.m_bufferSize = sizeof(ShadowPlaneConstants);
		shadowPlanesViewDesc.m_options = 0;
		shadowPlanesViewDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
		m_shadowPlanesBuffer = m_renderer->AllocateBuffer(shadowPlanesViewDesc);
		m_viewPlanesView = m_shadowPlanesBuffer->GetViewAsUniformBuffer();

		m_batchDispatcher = m_allocator->Alloc<BatchDispatcher>(m_renderer, m_allocator);
	}

	ShadowMapPass::~ShadowMapPass()
	{
		m_allocator->Free(m_batchDispatcher);

		m_renderer->FreeBuffer(m_shadowViewBuffer);
		m_shadowViewBuffer = nullptr;
		m_renderer->FreeBuffer(m_shadowPlanesBuffer);
		m_shadowPlanesBuffer = nullptr;
	}

	void ShadowMapPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("ShadowMapPass::OnPrePass", { 1.0f, 1.0f, 1.0f, 1.0f });

		if (m_shadowPipeline == 0)
		{
			PB::GraphicsPipelineDesc pipelineDesc = GetBasePipelineDesc(m_shadowmapResolution);
			{
				if (m_cascadeIndex == 0) // Disable culling on cascade 0 ONLY to reduce light leaking.
				{
					pipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
				}

				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::TASK] = Eng::Shader(m_renderer, "Shaders/GLSL/ts_obj_meshlet_cull", 0, m_allocator, true).GetModule();

				AssetEncoder::ShaderPermutationTable permTable{};
				permTable.SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_0, 1); // Output position only permutation

				pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::MESH] = Eng::Shader(m_renderer, "Shaders/GLSL/ms_obj_task_batch", permTable.GetKey(), m_allocator, true).GetModule();
			}

			m_shadowPipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
		}

		if (m_cascadeIndex > 0)
		{
			PB::ITexture* shadowmap = transientTextures[0];

			PB::SubresourceRange subresources{};
			subresources.m_firstArrayElement = m_cascadeIndex;
			info.m_commandContext->CmdTextureBarrier(shadowmap, PB::EMemoryBarrierType::GRAPHICS_ATTACHMENT_WRITE_TO_FRAGMENT_SHADER_READ, subresources);
		}

		m_hierarchyToDraw->GetDynamicDrawPool().UpdateComputeGPU(info.m_commandContext, m_viewPlanesView, m_mainCamViewPlanesView, true, true);
		m_hierarchyToDraw->GetStaticObjectRenderer().UpdateComputeGPU(info.m_commandContext, m_viewPlanesView, m_mainCamViewPlanesView, true);

		info.m_commandContext->CmdEndLastLabel();
	}

	void ShadowMapPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("ShadowMapPass", { 1.0f, 1.0f, 1.0f, 1.0f });

		if (m_shadowConstantsRequireUpdate)
		{
			memcpy(m_shadowViewBuffer->BeginPopulate(), &m_localShadowConstants, sizeof(ShadowConstants));
			m_shadowViewBuffer->EndPopulate();

			memcpy(m_shadowPlanesBuffer->BeginPopulate(), &m_localShadowPlaneConstants, sizeof(ShadowPlaneConstants));
			m_shadowPlanesBuffer->EndPopulate();
			m_shadowConstantsRequireUpdate = false;
		}

		info.m_commandContext->CmdSetViewport({ 0, 0, m_shadowmapResolution, m_shadowmapResolution }, 0.0f, 1.0f);
		info.m_commandContext->CmdSetScissor({ 0, 0, m_shadowmapResolution, m_shadowmapResolution });

		info.m_commandContext->CmdBindPipeline(m_shadowPipeline);
		m_hierarchyToDraw->GetDynamicDrawPool().Draw(info.m_commandContext, m_shadowConstantsView, m_viewPlanesView);
		m_hierarchyToDraw->GetStaticObjectRenderer().Draw(info.m_commandContext, m_shadowConstantsView, m_viewPlanesView);

		info.m_commandContext->CmdEndLastLabel();
	}

	void ShadowMapPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{

	}

	void ShadowMapPass::AddToRenderGraph(RenderGraphBuilder* builder, uint32_t shadowmapResolution)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = false;

		AttachmentDesc& depthDesc = nodeDesc.m_attachments.PushBackInit();
		depthDesc.m_format = PB::ETextureFormat::D16_UNORM;
		depthDesc.m_width = shadowmapResolution;
		depthDesc.m_height = shadowmapResolution;
		depthDesc.m_arraySize = m_cascadeIndex + 1;
		depthDesc.m_renderArrayLayer = m_cascadeIndex;
		depthDesc.m_name = "WorldShadowmap";
		depthDesc.m_usage = PB::EAttachmentUsage::DEPTHSTENCIL;
		depthDesc.m_clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		depthDesc.m_flags = EAttachmentFlags::CLEAR;

		TransientTextureDesc& depthReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		depthReadDesc.m_format = PB::ETextureFormat::D16_UNORM;
		depthReadDesc.m_width = shadowmapResolution;
		depthReadDesc.m_height = shadowmapResolution;
		depthReadDesc.m_arraySize = m_cascadeIndex + 1;
		depthReadDesc.m_name = "WorldShadowmap";
		depthReadDesc.m_initialUsage = PB::ETextureState::DEPTHTARGET;
		depthReadDesc.m_finalUsage = PB::ETextureState::DEPTHTARGET;
		depthReadDesc.m_usageFlags = PB::ETextureState::DEPTHTARGET;

		nodeDesc.m_renderWidth = depthDesc.m_width;
		nodeDesc.m_renderHeight = depthDesc.m_height;

		builder->AddNode(nodeDesc);
	}

	void ShadowMapPass::SetOutputTexture(PB::ITexture* tex)
	{
		m_outputTexture = tex;
	}

	void ShadowMapPass::SetCamera(const Camera* camera, PB::UniformBufferView cameraViewPlanesView, EntityHierarchy* hierarchyToDraw, const RenderBoundingVolumeHierarchy* rbvh, Vector3f viewDirection)
	{
		m_camera = camera;
		m_mainCamViewPlanesView = cameraViewPlanesView;
		m_hierarchyToDraw = hierarchyToDraw;
		m_rbvh = rbvh;

		m_localShadowConstants.m_shadowViewDirection = Vector4f(viewDirection.Normalized(), 0.0f);
	}

	void ShadowMapPass::Update()
	{
		Matrix4 axisCorrection;
		axisCorrection[1][1] = -1.0f;
		axisCorrection[2][2] = 1.0f;
		axisCorrection[3][3] = 1.0f;

		Camera::CameraFrustrum cascadeFrustrumSection;
		m_camera->GetFrustrumSection(cascadeFrustrumSection, m_frustrumSectionNear, m_frustrumSectionFar);

		Vector3f cascadeCentre(0.0f);
		for (const auto& corner : cascadeFrustrumSection.m_frustrumCorners)
			cascadeCentre += corner;

		constexpr size_t cornerCount = PB_ARRAY_LENGTH(cascadeFrustrumSection.m_frustrumCorners);
		cascadeCentre /= float(cornerCount);

		Vector3f normalizedViewDir = -m_localShadowConstants.m_shadowViewDirection;
		const float cascadeViewDistance = 100.0f;

		Vector3f eye = cascadeCentre - (normalizedViewDir * cascadeViewDistance);
		Vector3f up = m_camera->Up();
		Matrix4 viewMat = Matrix4::LookAt(eye, cascadeCentre, up);

		float frustrumSectionLength = m_frustrumSectionFar - m_frustrumSectionNear;
		float texelDistance = (frustrumSectionLength * 2) / m_shadowmapResolution;

		float minWidth = 0.0f;
		float minHeight = 0.0f;
		float maxWidth = 0.0f;
		float maxHeight = 0.0f;
		float maxDepth = 0.0f;
		for (auto& corner : cascadeFrustrumSection.m_frustrumCorners)
		{
			Vector4f viewSpaceCorner = viewMat * Vector4f(corner, 1.0f);
			minWidth = Min(minWidth, viewSpaceCorner.x);
			minHeight = Min(minHeight, viewSpaceCorner.y);
			maxWidth = Max(maxWidth, viewSpaceCorner.x);
			maxHeight = Max(maxHeight, viewSpaceCorner.y);
			maxDepth = Max(maxDepth, -viewSpaceCorner.z - cascadeViewDistance);
		}

		static constexpr float ShadowFrustrumScaleBias = 1.05f;

		minWidth *= ShadowFrustrumScaleBias;
		maxWidth *= ShadowFrustrumScaleBias;
		minHeight *= ShadowFrustrumScaleBias;
		maxHeight *= ShadowFrustrumScaleBias;

		const float farDistance = cascadeViewDistance + maxDepth;
		Matrix4 proj = axisCorrection * glm::orthoZO(minWidth, maxWidth, minHeight, maxHeight, 1.0f, farDistance);

		m_localShadowConstants.m_viewProjectionMatrix = proj * viewMat;

		Camera::GetShadowCascadeFrustrum(m_shadowCascadeFrustrum, eye, normalizedViewDir, up, minWidth, maxWidth, minHeight, maxHeight, 1.0f, farDistance);

#if SHADOW_MAP_PASS_DEBUG_DRAW_CASCADES
		if (!m_debugFrustrumIsSet)
		{
			for (const auto& corner : m_shadowCascadeFrustrum.m_frustrumCorners)
				cascadeCentre += corner;
			cascadeCentre /= float(PB_ARRAY_LENGTH(m_shadowCascadeFrustrum.m_frustrumCorners));

			m_debugCamFrustrum = cascadeFrustrumSection;
			m_debugCascadeFrustrum = m_shadowCascadeFrustrum;
			m_debugEye = eye;
			m_debugCascadeCenter = cascadeCentre;
			m_debugFrustrumIsSet = true;
		}
#endif

		m_localShadowPlaneConstants.m_shadowFrustrumPlanes[0] = m_shadowCascadeFrustrum.m_near;
		m_localShadowPlaneConstants.m_shadowFrustrumPlanes[1] = m_shadowCascadeFrustrum.m_left;
		m_localShadowPlaneConstants.m_shadowFrustrumPlanes[2] = m_shadowCascadeFrustrum.m_right;
		m_localShadowPlaneConstants.m_shadowFrustrumPlanes[3] = m_shadowCascadeFrustrum.m_top;
		m_localShadowPlaneConstants.m_shadowFrustrumPlanes[4] = m_shadowCascadeFrustrum.m_bottom;
		m_localShadowPlaneConstants.m_shadowFrustrumPlanes[5] = m_shadowCascadeFrustrum.m_far;
		m_localShadowPlaneConstants.m_cameraOrigin = eye;
		m_localShadowPlaneConstants.m_isOrthographic = 1;

		m_localShadowConstants.m_cascadeRange = Vector2f(m_frustrumSectionNear, m_frustrumSectionFar);

		m_localShadowConstants.m_shadowPenumbraDistance = m_softShadowPenumbraDistance / texelDistance;
		m_localShadowConstants.m_shadowBiasMultiplier = m_shadowBiasMultiplier;

		m_shadowConstantsRequireUpdate = true;
	}

	void ShadowMapPass::DebugDrawCascadeVolumes(DebugLinePass* linePass)
	{
#if SHADOW_MAP_PASS_DEBUG_DRAW_CASCADES
		// Not recommended to draw all of these at once, as it becomes rather cluttered-looking.

		linePass->DrawCube(m_debugEye - Vector3f(0.5f), Vector3f(1.0f), Vector3f(0.0f, 0.0f, 1.0f));
		linePass->DrawCube(m_debugCascadeCenter - Vector3f(0.5f), Vector3f(1.0f), Vector3f(1.0f, 0.0f, 0.0f));

		Camera::DrawFrustrum(linePass, m_debugCamFrustrum, Vector3f(0.0f, 1.0f, 0.0f));
		Camera::DrawFrustrum(linePass, m_debugCascadeFrustrum, Vector3f(1.0f, 0.0f, 1.0f));
#endif
	}

	PB::GraphicsPipelineDesc ShadowMapPass::GetBasePipelineDesc(uint32_t shadowMapResolution) const
	{
		assert(m_renderPass && "Cannot get optimal pipeline desc before adding this node to a RenderGraph.");

		PB::GraphicsPipelineDesc pipelineDesc{};
		pipelineDesc.m_renderPass = m_renderPass;
		pipelineDesc.m_subpass = 0;
		pipelineDesc.m_renderArea = { 0, 0, shadowMapResolution, shadowMapResolution };
		pipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL;
		pipelineDesc.m_cullMode = PB::EFaceCullMode::BACK;

		return pipelineDesc;
	}

	PB::BindingLayout ShadowMapPass::GetDrawBatchBindings()
	{
		PB::BindingLayout layout{};
		layout.m_uniformBufferCount = 1;
		layout.m_uniformBuffers = &m_shadowConstantsView;

		return layout;
	}

	PB::UniformBufferView ShadowMapPass::GetShadowConstantsView()
	{
		return m_shadowConstantsView;
	}
};