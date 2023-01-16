#pragma once
#include "RenderGraph/RenderGraphNode.h"

#include "Resource/Mesh.h"
#include "Resource/Texture.h"
#include "Resource/Shader.h"
#include "WorldRender/ObjectDispatcher.h"

namespace Eng
{
	class DrawBatch;
	class RenderGraphBuilder;
	class BatchDispatcher;
	class Camera;
	class RenderBoundingVolumeHierarchy;

	class GBufferPass : public RenderGraphBehaviour
	{
	public:

		GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator, PB::UniformBufferView viewConstView, PB::UniformBufferView viewPlanesView, const Camera* camera, RenderBoundingVolumeHierarchy* hierarchy);

		~GBufferPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder);

		void SetOutputTexture(PB::ITexture* tex);

		PB::GraphicsPipelineDesc GetBasePipelineDesc() const;

	private:

		// Dispatch list provided by the user, which draws geometry into the G Buffers.
		PB::ITexture* m_outputTexture = nullptr;
		BatchDispatcher* m_batchDispatcher = nullptr;

		PB::UniformBufferView m_viewConstView = nullptr;
		PB::UniformBufferView m_viewPlanesView = nullptr;
		const Camera* m_camera = nullptr;
		const RenderBoundingVolumeHierarchy* m_rbvh = nullptr;
		PB::BindingLayout m_batchBindings;
		PB::Pipeline m_drawbatchPipeline = 0;
	};
};