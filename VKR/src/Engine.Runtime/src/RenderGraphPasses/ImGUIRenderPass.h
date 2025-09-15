#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Shader.h"

struct ImDrawData;

namespace Eng
{
	class RenderGraphBuilder;

	class ImGUIRenderPass : public RenderGraphBehaviour
	{
	public:

		ImGUIRenderPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

		~ImGUIRenderPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder);

		void SetDrawData(ImDrawData* drawData) { m_drawData = drawData; }

		void SetOutputTexture(PB::ITexture* tex);

	private:

		ImDrawData* m_drawData = nullptr;
		PB::ITexture* m_outputTexture = nullptr;
	};
};