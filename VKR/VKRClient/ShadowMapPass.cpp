#include "ShadowMapPass.h"
#include "RenderGraph.h"

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT
#include "glm/gtc/matrix_transform.hpp"
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
}

ShadowMapPass::~ShadowMapPass()
{
	m_renderer->FreeBuffer(m_shadowViewBuffer);
	m_shadowViewBuffer = nullptr;
}

void ShadowMapPass::OnPrePass(const RenderGraphInfo& info)
{
	if (m_listRequiresUpdate)
	{
		m_geoDispatchList->Update(info.m_commandContext);
		m_listRequiresUpdate = false;
	}
}

void ShadowMapPass::OnPassBegin(const RenderGraphInfo& info)
{
	if (m_shadowConstantsRequireUpdate)
	{
		memcpy(m_shadowViewBuffer->BeginPopulate(), &m_localShadowConstants, sizeof(ShadowConstants));
		m_shadowViewBuffer->EndPopulate();
		m_shadowConstantsRequireUpdate = false;
	}

	if (m_geoDispatchList)
		m_geoDispatchList->Dispatch(info.m_commandContext, info.m_renderPass, info.m_frameBuffer);
}

void ShadowMapPass::OnPostPass(const RenderGraphInfo& info)
{
	if (!m_outputTexture)
		return;

	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[0], PB::ETextureState::COLORTARGET, PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT, PB::ETextureState::COPY_DST);
	
	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(info.m_renderTargets[0], m_outputTexture);
	
	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
}

void ShadowMapPass::AddToRenderGraph(RenderGraphBuilder* builder, uint32_t shadowmapResolution)
{
	NodeDesc nodeDesc;
	nodeDesc.m_behaviour = this;
	nodeDesc.m_useReusableCommandLists = true;

	AttachmentDesc& depthDesc = nodeDesc.m_attachments[0];
	depthDesc.m_format = PB::ETextureFormat::D16_UNORM;
	depthDesc.m_width = shadowmapResolution;
	depthDesc.m_height = shadowmapResolution;
	depthDesc.m_name = "WorldShadowmap";
	depthDesc.m_usage = PB::EAttachmentUsage::DEPTHSTENCIL;
	depthDesc.m_clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	depthDesc.m_flags = EAttachmentFlags::CLEAR;

	nodeDesc.m_attachmentCount = 1;
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

void ShadowMapPass::SetViewMatrix(float* matrix)
{
	memcpy(m_localShadowConstants.m_viewMatrix, matrix, sizeof(float) * 16);

	glm::mat4 viewAsMat4;;
	memcpy(&viewAsMat4, matrix, sizeof(float) * 16);
	viewAsMat4 = glm::inverse(viewAsMat4);

	m_localShadowConstants.m_shadowViewDirection = PB::Float3(viewAsMat4[3].x, viewAsMat4[3].y, viewAsMat4[3].z);
	m_shadowConstantsRequireUpdate = true;
}

void ShadowMapPass::SetShadowParameters(float distance, float softShadowPenumbraDistance, float biasMultiplier, uint32_t resolution)
{
	glm::mat4 axisCorrection;
	axisCorrection[1][1] = -1.0f;
	axisCorrection[2][2] = 1.0f;
	axisCorrection[3][3] = 1.0f;
	glm::mat4 shadowProj = axisCorrection * glm::ortho<float>(-distance, distance, -distance, distance, 0.0f, 100.0f);

	float texelDistance = (distance * 2) / resolution;
	m_localShadowConstants.m_shadowPenumbraDistance = (softShadowPenumbraDistance * biasMultiplier) / texelDistance;
	m_localShadowConstants.m_shadowBiasMultiplier = biasMultiplier;

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
