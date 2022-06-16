#include "ShadowMapPass.h"
#include "RenderGraph.h"
#include "Camera.h"
#include "RenderBoundingVolumeHierarchy.h"
#include "BatchDispatcher.h"

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#pragma warning(pop)

using namespace PBClient;

ShadowMapPass::ShadowMapPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;

	PB::BufferObjectDesc shadowViewDesc;
	shadowViewDesc.m_bufferSize = sizeof(ShadowConstants);
	shadowViewDesc.m_options = 0;
	shadowViewDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
	m_shadowViewBuffer = m_renderer->AllocateBuffer(shadowViewDesc);
	m_svbView = m_shadowViewBuffer->GetViewAsUniformBuffer();

	m_batchBindings.m_uniformBufferCount = 1;
	m_batchBindings.m_uniformBuffers = &m_svbView;
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

	if (m_listRequiresUpdate)
	{
		m_geoDispatchList->Update(info.m_commandContext);
		m_listRequiresUpdate = false;
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

	//if (m_geoDispatchList)
	//	m_geoDispatchList->Dispatch(info.m_commandContext, info.m_renderPass, info.m_frameBuffer);

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
	depthDesc.m_name = "WorldShadowmap";
	depthDesc.m_usage = PB::EAttachmentUsage::DEPTHSTENCIL;
	depthDesc.m_clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	depthDesc.m_flags = EAttachmentFlags::CLEAR;

	nodeDesc.m_renderWidth = depthDesc.m_width;
	nodeDesc.m_renderHeight = depthDesc.m_height;

	builder->AddNode(nodeDesc);
}

void ShadowMapPass::SetDispatchList(ObjectDispatchList* list, bool updateList)
{
	m_geoDispatchList = list;
	m_listRequiresUpdate |= updateList;
}

void ShadowMapPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

void ShadowMapPass::SetCamera(const Camera* camera, const RenderBoundingVolumeHierarchy* rbvh)
{
	m_camera = camera;
	m_rbvh = rbvh;

	PB::BufferViewDesc viewPlanesDesc;
	viewPlanesDesc.m_buffer = m_shadowViewBuffer;
	viewPlanesDesc.m_offset = offsetof(ShadowConstants, ShadowConstants::m_shadowFrustrumPlanes);
	viewPlanesDesc.m_size = sizeof(ShadowConstants::m_shadowFrustrumPlanes);
	m_viewPlanesView = m_shadowViewBuffer->GetViewAsUniformBuffer(viewPlanesDesc);

	glm::mat4 viewMat = camera->GetViewMatrix();
	memcpy(m_localShadowConstants.m_viewMatrix, glm::value_ptr(viewMat), sizeof(glm::mat4));

	const Camera::CameraFrustrum& frustrum = camera->GetFrustrum();
	memcpy(&m_localShadowConstants.m_shadowFrustrumPlanes[0], glm::value_ptr(frustrum.m_near), sizeof(Camera::CameraFrustrum::Plane));
	memcpy(&m_localShadowConstants.m_shadowFrustrumPlanes[4], glm::value_ptr(frustrum.m_left), sizeof(Camera::CameraFrustrum::Plane));
	memcpy(&m_localShadowConstants.m_shadowFrustrumPlanes[8], glm::value_ptr(frustrum.m_right), sizeof(Camera::CameraFrustrum::Plane));
	memcpy(&m_localShadowConstants.m_shadowFrustrumPlanes[12], glm::value_ptr(frustrum.m_top), sizeof(Camera::CameraFrustrum::Plane));
	memcpy(&m_localShadowConstants.m_shadowFrustrumPlanes[16], glm::value_ptr(frustrum.m_bottom), sizeof(Camera::CameraFrustrum::Plane));
	memcpy(&m_localShadowConstants.m_shadowFrustrumPlanes[20], glm::value_ptr(frustrum.m_far), sizeof(Camera::CameraFrustrum::Plane));

	m_localShadowConstants.m_shadowViewDirection = PB::Float4(viewMat[3].x, viewMat[3].y, viewMat[3].z, 0.0f);
	m_shadowConstantsRequireUpdate = true;
}

void ShadowMapPass::SetShadowParameters(float distance, float softShadowPenumbraDistance, float biasMultiplier, uint32_t resolution)
{
	assert(m_camera != nullptr);

	m_shadowmapResolution = resolution;

	float texelDistance = (distance * 2) / resolution;
	m_localShadowConstants.m_shadowPenumbraDistance = softShadowPenumbraDistance / texelDistance;
	m_localShadowConstants.m_shadowBiasMultiplier = biasMultiplier;

	glm::mat4 shadowProj = m_camera->GetProjectionMatrix();
	memcpy(m_localShadowConstants.m_projMatrix, &shadowProj, sizeof(float) * 16);
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
	layout.m_uniformBuffers = &m_svbView;

	return layout;
}

PB::UniformBufferView ShadowMapPass::GetSVBView()
{
	return m_svbView;
}
