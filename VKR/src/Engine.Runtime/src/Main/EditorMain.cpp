#include "EditorMain.h"

#include "RenderGraphPasses/ImGUIRenderPass.h"

#include "imgui_internal.h"

namespace Eng
{
	using namespace Math;

	EditorMain::EditorMain(PB::IRenderer* renderer, CLib::Allocator* allocator)
		: m_renderer(renderer)
		, m_allocator(allocator)
	{
		m_viewportPos = Vector2f(0.5f, 0.35f);
		m_viewportScale.x = 0.6f;
		m_viewportScale.y = m_viewportPos.y * 2.0f;
		m_viewportPos = CenterRectPosition(m_viewportPos, m_viewportScale);

		UpdateResolution();
	}

	EditorMain::~EditorMain()
	{
		if (m_editorRenderGraph)
		{
			m_allocator->Free(m_editorRenderGraph);
			m_editorRenderGraph = nullptr;
		}
		if (m_imguiRenderPass)
		{
			m_allocator->Free(m_imguiRenderPass);
			m_imguiRenderPass = nullptr;
		}

		if (m_editorRenderOutput)
		{
			m_renderer->FreeTexture(m_editorRenderOutput);
			m_editorRenderOutput = nullptr;
		}
	}

	void EditorMain::UpdateResolution()
	{
		PB::ISwapChain* swapchain = m_renderer->GetSwapchain();
		m_windowDimensions.x = float(swapchain->GetWidth());
		m_windowDimensions.y = float(swapchain->GetHeight());

		CreateRenderGraph();
	}

	void EditorMain::UpdateGUI(const PB::ImGuiTextureData& worldRenderOutput)
	{
		ImGui::NewFrame();

		ViewportImGUI(&worldRenderOutput);

		ImGui::Render();
		m_drawData = ImGui::GetDrawData();

	}

	void EditorMain::RenderGUI(PB::ICommandContext* cmdContext)
	{
		if (m_editorRenderGraph)
		{
			PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
			auto* swapChainTex = m_renderer->GetSwapchain()->GetImage(swapChainIdx);

			m_imguiRenderPass->SetDrawData(m_drawData);
			m_imguiRenderPass->SetOutputTexture(swapChainTex);

			m_editorRenderGraph->Execute(cmdContext);
		}
	}

	Math::Vector2u EditorMain::GetViewportOrigin()
	{
		const uint32_t titleHeight = 19; // #1 problem with ImGui, it does not have an API to calculate window sizes in advance before creating them. So we have to guess the title bar height...

		Vector2f viewPortOriginScaled = m_viewportPos * m_windowDimensions;
		return Vector2u(uint32_t(viewPortOriginScaled.x), uint32_t(viewPortOriginScaled.y) + titleHeight);
	}

	Math::Vector2u EditorMain::GetViewportResolution()
	{
		ImGuiStyle style{};
		
		// Account for border/padding and title bar.
		Vector2f windowContentDimensions = m_windowDimensions;
		windowContentDimensions -= 2 * style.WindowBorderSize;

		const float heightReduction = 25.0f; // Sigh... Once again, we can't calculate window math in advance with ImGui, so here we are guessing it again.
		windowContentDimensions.y -= heightReduction;

		Vector2f resolution = m_viewportScale * windowContentDimensions;
		return Vector2u(uint32_t(resolution.x), uint32_t(resolution.y));
	}

	void EditorMain::CreateRenderGraph()
	{
		if (m_editorRenderGraph)
		{
			m_allocator->Free(m_editorRenderGraph);
		}

		RenderGraphBuilder rgBuilder(m_renderer, m_allocator);

		// ImGUI Pass
		{
			if (!m_imguiRenderPass)
				m_imguiRenderPass = m_allocator->Alloc<ImGUIRenderPass>(m_renderer, m_allocator);
			m_imguiRenderPass->AddToRenderGraph(&rgBuilder);
		}

		if (m_editorRenderOutput != nullptr)
		{
			m_renderer->FreeTexture(m_editorRenderOutput);
		}

		PB::ISwapChain* swapchain = m_renderer->GetSwapchain();

		constexpr const char* OutputRGName = "MergedOutput";
		const auto& outputData = rgBuilder.GetTextureData(OutputRGName);

		PB::TextureDesc editorRenderOutputDesc{};
		editorRenderOutputDesc.m_name = "EditorRender_Output";
		editorRenderOutputDesc.m_format = swapchain->GetImageFormat();
		editorRenderOutputDesc.m_width = swapchain->GetWidth();
		editorRenderOutputDesc.m_height = swapchain->GetHeight();
		editorRenderOutputDesc.m_usageStates = outputData->m_usage | PB::ETextureState::COPY_SRC;
		editorRenderOutputDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_NONE;
		m_editorRenderOutput = m_renderer->AllocateTexture(editorRenderOutputDesc);
		rgBuilder.RegisterUserTexture("MergedOutput", m_editorRenderOutput);

		m_editorRenderGraph = rgBuilder.Build(false);
	}

	void EditorMain::ViewportImGUI(const PB::ImGuiTextureData* worldRenderOutput)
	{
		ImVec2 windowSize = (ImVec2)(m_viewportScale * m_windowDimensions);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);

		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		{
			ImGui::SetWindowSize(windowSize);
			ImGui::SetWindowPos((ImVec2)(m_viewportPos * m_windowDimensions));

			if (worldRenderOutput)
			{
				ImGui::Image(worldRenderOutput->ref, ImVec2(float(worldRenderOutput->width), float(worldRenderOutput->height)));
			}
		}
		ImGui::End();

		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
	}

	Vector2f EditorMain::CenterRectPosition(const Vector2f position, const Vector2f& dimensions)
	{
		return (position - (dimensions * 0.5));
	}
}
