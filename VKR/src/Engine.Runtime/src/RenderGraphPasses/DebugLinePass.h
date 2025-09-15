#pragma once
#include "RenderGraph/RenderGraphNode.h"

#include <Engine.Math/Vectors.h>

namespace Eng
{
	class DebugLinePass : public RenderGraphBehaviour
	{
	public:

		DebugLinePass(PB::IRenderer* renderer, CLib::Allocator* allocator, PB::UniformBufferView mvpView);

		~DebugLinePass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(class RenderGraphBuilder* builder, Math::Vector2u targetResolution);

		void DrawLine(PB::Float3 startPoint, PB::Float3 endPoint, PB::Float3 color);
		void DrawLine(Math::Vector3f startPoint, Math::Vector3f endPoint, Math::Vector3f color);
		void DrawLine(PB::Float4 startPoint, PB::Float4 endPoint, PB::Float4 color);
		void DrawCube(const Math::Vector3f& origin, const Math::Vector3f& extents, const Math::Vector3f& lineColor);

	private:

		static constexpr const uint32_t MaxLineCount = 4096 * 16;

		struct LineVertex
		{
			PB::Float4 m_position;
			PB::Float4 m_color;
		};

		Math::Vector2u m_targetResolution{};
		PB::Pipeline m_linePipeline = 0;
		PB::UniformBufferView m_mvpView = nullptr;
		uint32_t m_activeLineCount = 0;
		PB::IBufferObject* m_localLineVertexBuffer = nullptr;
		PB::IBufferObject* m_lineVertexBuffer = nullptr;
		LineVertex* m_mappedLocalLineVertexBuffer = nullptr;
	};
};