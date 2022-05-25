#pragma once
#include "RenderGraphNode.h"

#include "Mesh.h"
#include "Texture.h"
#include "Shader.h"
#include "ObjectDispatcher.h"

class DrawBatch;
class RenderGraphBuilder;

class GBufferPass : public RenderGraphBehaviour
{
public:

	GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator, PB::UniformBufferView viewConstView, DrawBatch* testBatch);

	~GBufferPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	void SetDispatchList(ObjectDispatchList* list, bool updateList);

	void SetOutputTexture(PB::ITexture* tex);

	PB::GraphicsPipelineDesc GetBasePipelineDesc() const;

private:

	// Dispatch list provided by the user, which draws geometry into the G Buffers.
	PB::ITexture* m_outputTexture = nullptr;
	ObjectDispatchList* m_geoDispatchList = nullptr;
	bool m_listRequiresUpdate = false;

	PB::UniformBufferView m_viewConstView = nullptr;
	PB::Pipeline m_drawbatchPipeline = 0;
	DrawBatch* m_experimentalDB = nullptr;
	PB::ICommandList* m_expCmdList = nullptr;
};

