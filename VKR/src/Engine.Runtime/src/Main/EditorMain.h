#pragma once
#include "Engine.ParaBlit/ISwapChain.h"
#include "Engine.ParaBlit/IImGUIModule.h"
#include "RenderGraph/RenderGraph.h"

#define MATH_IMPL_IMGUI
#include "Engine.Math/Vector2.h"

#include "imgui.h"

#include <cstdint>

namespace Eng
{
	class EditorMain
	{
	public:

		EditorMain(PB::IRenderer* renderer, CLib::Allocator* allocator);

		~EditorMain();

		void UpdateResolution();

		void UpdateGUI(const PB::ImGuiTextureData& worldRenderOutput);

		void RenderGUI(PB::ICommandContext* cmdContext);

		Math::Vector2u GetViewportOrigin();
		Math::Vector2u GetViewportResolution();

	private:

		void CreateRenderGraph();
		void ViewportImGUI(const PB::ImGuiTextureData* worldRenderOutput);

		PB::IRenderer* m_renderer;
		CLib::Allocator* m_allocator;

		RenderGraph* m_editorRenderGraph = nullptr;
		PB::ITexture* m_editorRenderOutput = nullptr;
		class ImGUIRenderPass* m_imguiRenderPass = nullptr;
		ImDrawData* m_drawData = nullptr;

		Math::Vector2f m_windowDimensions = Math::Vector2f(1280.0f, 720.0f);

		Math::Vector2f m_viewportPos{};
		Math::Vector2f m_viewportScale{};

		Math::Vector2f CenterRectPosition(const Math::Vector2f position, const Math::Vector2f& dimensions);
	};
}