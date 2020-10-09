#pragma once
#include "RenderGraphNode.h"

#include "Mesh.h"
#include "Texture.h"
#include "Shader.h"

class GBufferPass : public RenderGraphBehaviour
{
public:

	GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~GBufferPass();

	void OnPreRenderPass(const RenderGraphInfo& info) override;

	void OnPassBegin(const RenderGraphInfo& info) override;

	void OnPostRenderPass(const RenderGraphInfo& info) override;

	void SetOutputTexture(PB::ITexture* tex);

	void SetMVPBuffer(PB::IBufferObject* buffer);

	void SetInstanceBuffer(PB::IBufferObject* buffer);

private:

	PB::IBufferObject* m_mvpBuffer = nullptr;
	PB::IBufferObject* m_instanceBuffer = nullptr;

	PBClient::Mesh* m_paintMesh = nullptr;
	PBClient::Mesh* m_detailsMesh = nullptr;
	PBClient::Mesh* m_glassMesh = nullptr;

	PBClient::Texture* m_paintTexture = nullptr;
	PBClient::Texture* m_detailsTexture = nullptr;
	PBClient::Texture* m_glassTexture = nullptr;

	PB::TextureView m_paintView = 0;
	PB::TextureView m_detailsView = 0;
	PB::TextureView m_glassView = 0;

	PB::Sampler m_colorSampler = 0;

	PBClient::Shader* m_vertShader = nullptr;
	PBClient::Shader* m_fragShader = nullptr;
	PB::Pipeline m_pipeline = 0;

	PB::ITexture* m_outputTexture = nullptr;
};

