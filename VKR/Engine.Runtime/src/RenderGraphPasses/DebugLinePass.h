#pragma once
#include "RenderGraph/RenderGraphNode.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

class DebugLinePass : public RenderGraphBehaviour
{
public:

	DebugLinePass(PB::IRenderer* renderer, CLib::Allocator* allocator, PB::UniformBufferView mvpView);

	~DebugLinePass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(class RenderGraphBuilder* builder);

	void DrawLine(PB::Float3 startPoint, PB::Float3 endPoint, PB::Float3 color);
	void DrawLine(glm::vec3 startPoint, glm::vec3 endPoint, glm::vec3 color);
	void DrawLine(PB::Float4 startPoint, PB::Float4 endPoint, PB::Float4 color);

private:

	static constexpr const uint32_t MaxLineCount = 4096 * 16;

	struct LineVertex
	{
		PB::Float4 m_position;
		PB::Float4 m_color;
	};

	PB::Pipeline m_linePipeline = 0;
	PB::UniformBufferView m_mvpView = nullptr;
	uint32_t m_activeLineCount = 0;
	PB::IBufferObject* m_localLineVertexBuffer = nullptr;
	PB::IBufferObject* m_lineVertexBuffer = nullptr;
	LineVertex* m_mappedLocalLineVertexBuffer = nullptr;
};