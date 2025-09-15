#pragma once
#include "IImGUIModule.h"

#include "backends/imgui_impl_vulkan.h"

namespace PB
{
	class Renderer;

	class ImGUIModule : public IImGUIModule
	{
	public:

		ImGUIModule(Renderer* renderer, ImGuiContext* imguiContext);

		~ImGUIModule();

		void RenderDrawData(ImDrawData* drawData, ICommandContext* cmdContext, Pipeline pipeline) override;

		void AddTexture(ImGuiTextureData& outData, ITexture* texture, const TextureViewDesc& viewDesc, const SamplerDesc& samplerDesc) override;

		void RemoveTexture(ImGuiTextureData& data) override;

	private:

		RenderPass CreateImGUIRenderPass();

		ImGui_ImplVulkan_InitInfo m_initInfo{};
		Renderer* m_renderer = nullptr;
	};
}