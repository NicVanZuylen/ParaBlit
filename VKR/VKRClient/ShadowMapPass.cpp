#include "ShadowMapPass.h"
#include "RenderGraph.h"
#include "RenderBoundingVolumeHierarchy.h"
#include "BatchDispatcher.h"

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#pragma warning(pop)

using namespace PBClient;

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

	PB::BufferViewDesc viewPlanesDesc;
	viewPlanesDesc.m_buffer = m_shadowViewBuffer;
	viewPlanesDesc.m_offset = offsetof(ShadowConstants, ShadowConstants::m_shadowFrustrumPlanes);
	viewPlanesDesc.m_size = sizeof(ShadowConstants::m_shadowFrustrumPlanes);
	m_viewPlanesView = m_shadowViewBuffer->GetViewAsUniformBuffer(viewPlanesDesc);

	m_batchBindings.m_uniformBufferCount = 1;
	m_batchBindings.m_uniformBuffers = &m_shadowConstantsView;
	m_batchBindings.m_resourceCount = 0;
	m_batchBindings.m_resourceViews = nullptr;

	m_batchDispatcher = m_allocator->Alloc<BatchDispatcher>(m_renderer, m_allocator);
}

ShadowMapPass::~ShadowMapPass()
{
	m_allocator->Free(m_batchDispatcher);

	m_renderer->FreeBuffer(m_shadowViewBuffer);
	m_shadowViewBuffer = nullptr;
}

void ShadowMapPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	if (m_shadowPipeline == 0)
	{
		PB::GraphicsPipelineDesc pipelineDesc = GetBasePipelineDesc(m_shadowmapResolution);
		pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = PBClient::Shader(m_renderer, "Shaders/GLSL/vs_obj_shad_batch", m_allocator, true).GetModule();

		m_shadowPipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
	}

	m_rbvh->CullBatches(m_camera, m_batchDispatcher, m_batchBindings);
	m_batchDispatcher->DispatchFrustrumCull(info.m_commandContext, m_viewPlanesView);
}

void ShadowMapPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	if (m_shadowConstantsRequireUpdate)
	{
		memcpy(m_shadowViewBuffer->BeginPopulate(), &m_localShadowConstants, sizeof(ShadowConstants));
		m_shadowViewBuffer->EndPopulate();
		m_shadowConstantsRequireUpdate = false;
	}

	info.m_commandContext->CmdSetViewport({ 0, 0, m_shadowmapResolution, m_shadowmapResolution }, 0.0f, 1.0f);
	info.m_commandContext->CmdSetScissor({ 0, 0, m_shadowmapResolution, m_shadowmapResolution });

	m_batchDispatcher->DrawBatches(info.m_commandContext, m_shadowPipeline);
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

	nodeDesc.m_renderWidth = depthDesc.m_width;
	nodeDesc.m_renderHeight = depthDesc.m_height;

	builder->AddNode(nodeDesc);
}

void ShadowMapPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

void ShadowMapPass::SetCamera(const Camera* camera, const RenderBoundingVolumeHierarchy* rbvh, glm::vec3 viewDirection)
{
	m_camera = camera;
	m_rbvh = rbvh;

	m_localShadowConstants.m_shadowViewDirection = glm::vec4(glm::normalize(viewDirection), 0.0f);
}

void ShadowMapPass::Update()
{
	glm::mat4 axisCorrection;
	axisCorrection[1][1] = -1.0f;
	axisCorrection[2][2] = 1.0f;
	axisCorrection[3][3] = 1.0f;

	Camera::CameraFrustrum cascadeFrustrumSection;
	m_camera->GetFrustrumSection(cascadeFrustrumSection, m_frustrumSectionNear, m_frustrumSectionFar);

	glm::vec3 frustrumCorners[8] =
	{
		cascadeFrustrumSection.m_nearBottomLeft,
		cascadeFrustrumSection.m_nearBottomRight,
		cascadeFrustrumSection.m_nearTopLeft,
		cascadeFrustrumSection.m_nearTopRight,
		cascadeFrustrumSection.m_farBottomLeft,
		cascadeFrustrumSection.m_farBottomRight,
		cascadeFrustrumSection.m_farTopLeft,
		cascadeFrustrumSection.m_farTopRight
	};

	glm::vec3 cascadeCentre(0.0f);
	for (const auto& corner : frustrumCorners)
		cascadeCentre += corner;
	cascadeCentre /= 8;

	glm::vec3 normalizedViewDir = m_localShadowConstants.m_shadowViewDirection;
	const float cascadeViewDistance = 100.0f;

	Camera::CreateDesc cascadeCamDesc;
	cascadeCamDesc.m_projectionType = Camera::EProjectionType::ORTHOGRAPHIC;
	cascadeCamDesc.m_position = cascadeCentre + (normalizedViewDir * cascadeViewDistance);
	cascadeCamDesc.m_eulerAngles = glm::radians(glm::vec3(-45.0f, 0.0f, 0.0f));
	cascadeCamDesc.m_zNear = 1.0f;
	cascadeCamDesc.m_zFar = 100.0f;
	m_cascadeCamera = Camera(cascadeCamDesc);

	glm::mat4 viewMat = m_cascadeCamera.GetViewMatrix();

	//float shadowmapDistance = 0.0f;
	float minWidth = 0.0f;
	float minHeight = 0.0f;
	float maxWidth = 0.0f;
	float maxHeight = 0.0f;
	float maxDepth = 0.0f;
	for (auto& corner : frustrumCorners)
	{
		glm::vec4 viewSpaceCorner = viewMat * glm::vec4(corner, 1.0f);
		minWidth = glm::min(minWidth, viewSpaceCorner.x);
		minHeight = glm::min(minHeight, viewSpaceCorner.y);
		maxWidth = glm::max(maxWidth, viewSpaceCorner.x);
		maxHeight = glm::max(maxHeight, viewSpaceCorner.y);
		maxDepth = glm::max(maxDepth, -viewSpaceCorner.z - cascadeViewDistance);
	}

	float mapWidth = maxWidth - minWidth;
	float mapHeight = maxHeight - minHeight;

	m_cascadeCamera.SetWidth(mapWidth);
	m_cascadeCamera.SetHeight(mapHeight);
	m_cascadeCamera.SetZFarDistance(m_cascadeCamera.ZFar() + maxDepth);
	m_cascadeCamera.Update();

	m_localShadowConstants.m_viewProjectionMatrix = m_cascadeCamera.GetProjectionMatrix() * viewMat;
	const Camera::CameraFrustrum& cascadeFrustrum = m_cascadeCamera.GetFrustrum();

	m_localShadowConstants.m_shadowFrustrumPlanes[0] = cascadeFrustrum.m_near;
	m_localShadowConstants.m_shadowFrustrumPlanes[1] = cascadeFrustrum.m_left;
	m_localShadowConstants.m_shadowFrustrumPlanes[2] = cascadeFrustrum.m_right;
	m_localShadowConstants.m_shadowFrustrumPlanes[3] = cascadeFrustrum.m_top;
	m_localShadowConstants.m_shadowFrustrumPlanes[4] = cascadeFrustrum.m_bottom;
	m_localShadowConstants.m_shadowFrustrumPlanes[5] = cascadeFrustrum.m_far;

	m_localShadowConstants.m_cascadeRange = glm::vec2(m_frustrumSectionNear, m_frustrumSectionFar);

	//float texelDistance = (shadowmapDistance * 2) / m_shadowmapResolution;
	float frustrumSectionLength = m_frustrumSectionFar - m_frustrumSectionNear;
	float texelDistance = (frustrumSectionLength * 2) / m_shadowmapResolution;
	m_localShadowConstants.m_shadowPenumbraDistance = m_softShadowPenumbraDistance / texelDistance;
	m_localShadowConstants.m_shadowBiasMultiplier = m_shadowBiasMultiplier;

	m_shadowConstantsRequireUpdate = true;
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
