#pragma once
#include "RenderGraphNode.h"

#include "Mesh.h"
#include "Texture.h"
#include "Shader.h"
#include "ObjectDispatcher.h"

class DrawBatch;

class GBufferPass : public RenderGraphBehaviour
{
public:

	GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~GBufferPass();

	void OnPrePass(const RenderGraphInfo& info) override;

	void OnPassBegin(const RenderGraphInfo& info) override;

	void OnPostPass(const RenderGraphInfo& info) override;

	void SetDispatchList(ObjectDispatchList* list, bool updateList);

	void SetOutputTexture(PB::ITexture* tex);

	PB::GraphicsPipelineDesc GetBasePipelineDesc(PB::ISwapChain* swapChain) const;

private:

	// Dispatch list provided by the user, which draws geometry into the G Buffers.
	PB::ITexture* m_outputTexture = nullptr;
	ObjectDispatchList* m_geoDispatchList = nullptr;
	bool m_listRequiresUpdate = false;
};

