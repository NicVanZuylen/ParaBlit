#pragma once
#include "RenderGraphNode.h"

#include "Mesh.h"
#include "Texture.h"
#include "Shader.h"
#include "ObjectDispatcher.h"

class DrawBatch;
class RenderGraphBuilder;

class ShadowMapPass : public RenderGraphBehaviour
{
public:

	ShadowMapPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~ShadowMapPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder, uint32_t shadowmapResolution);

	void SetDispatchList(ObjectDispatchList* list, bool updateList);

	void SetOutputTexture(PB::ITexture* tex);

	void SetViewMatrix(float* matrix);

	void SetShadowParameters(float distance, float softShadowPenumbraDistance, float biasMultiplier, uint32_t resolution);

	PB::GraphicsPipelineDesc GetBasePipelineDesc(uint32_t shadowMapResolution) const;

	PB::BindingLayout GetDrawBatchBindings();

	PB::UniformBufferView GetSVBView();

private:

	struct ShadowConstants
	{
		float m_viewMatrix[16];
		float m_projMatrix[16];
		PB::Float3 m_shadowViewDirection;
		float m_shadowPenumbraDistance;
		float m_shadowBiasMultiplier;
		float m_pad[3];
	} m_localShadowConstants{};
	
	PB::IBufferObject* m_shadowViewBuffer = nullptr;
	PB::UniformBufferView m_svbView = 0;
	bool m_shadowConstantsRequireUpdate = false;

	PB::ITexture* m_outputTexture = nullptr;
	ObjectDispatchList* m_geoDispatchList = nullptr;
	bool m_listRequiresUpdate = false;

	uint32_t m_shadowmapResolution = 0;
};

