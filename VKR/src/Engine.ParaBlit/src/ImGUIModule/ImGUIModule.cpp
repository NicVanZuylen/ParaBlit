#include "ImGUIModule.h"

#include "../Renderer.h"
#include "../PBUtil.h"
#include "ParaBlitDefs.h"
#include "ParaBlitImplUtil.h"

namespace PB
{
	ImGUIModule::ImGUIModule(Renderer* renderer, ImGuiContext* imguiContext)
	{
		m_renderer = renderer;

		Device* device = renderer->GetDevice();

		m_initInfo.ApiVersion = VK_HEADER_VERSION_COMPLETE;
		m_initInfo.Instance = renderer->GetInstance().GetHandle();
		m_initInfo.PhysicalDevice = device->GetPhysicalDevice();
		m_initInfo.Device = device->GetHandle();
		m_initInfo.QueueFamily = device->GetGraphicsQueueFamilyIndex();
		
		vkGetDeviceQueue(device->GetHandle(), m_initInfo.QueueFamily, 0, &m_initInfo.Queue);

		m_initInfo.DescriptorPool = VK_NULL_HANDLE;
		m_initInfo.RenderPass = static_cast<VkRenderPass>(CreateImGUIRenderPass());
		m_initInfo.MinImageCount = m_renderer->GetSwapchain()->GetImageCount();
		m_initInfo.ImageCount = m_initInfo.MinImageCount + 1;
		m_initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		m_initInfo.DescriptorPoolSize = 8;
		
		ImGui::SetCurrentContext(imguiContext);
		ImGui_ImplVulkan_Init(&m_initInfo);
	}

	ImGUIModule::~ImGUIModule()
	{
		ImGui_ImplVulkan_Shutdown();
	}

	void ImGUIModule::RenderDrawData(ImDrawData* drawData, ICommandContext* cmdContext, Pipeline pipeline)
	{
		PipelineData* pipelineData = reinterpret_cast<PipelineData*>(pipeline);
		CommandContext* cmdContextInternal = reinterpret_cast<CommandContext*>(cmdContext);

		VkPipeline pipelineHandle = pipelineData ? pipelineData->m_pipeline : nullptr;
		ImGui_ImplVulkan_RenderDrawData(drawData, cmdContextInternal->GetCmdBuffer(), pipelineHandle);
	}

	void ImGUIModule::AddTexture(ImGuiTextureData& outData, ITexture* texture, const TextureViewDesc& viewDesc, const SamplerDesc& samplerDesc)
	{
		Texture* texInternal = reinterpret_cast<Texture*>(texture);

		VkImageView view = m_renderer->GetViewCache()->GetVkImageView(viewDesc);
		VkSampler sampler = m_renderer->GetViewCache()->GetVkSampler(samplerDesc);
		VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(sampler, view, ConvertPBStateToImageLayout(viewDesc.m_expectedState));

		outData.ref._TexID = reinterpret_cast<ImTextureID>(ds);

		VkExtent3D extent = texInternal->GetExtent();
		outData.width = extent.width;
		outData.height = extent.height;
	}

	void ImGUIModule::RemoveTexture(ImGuiTextureData& data)
	{
		ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(data.ref._TexID));
	}

	RenderPass ImGUIModule::CreateImGUIRenderPass()
	{
		RenderPassDesc passDesc{};
		passDesc.m_attachmentCount = 1;
		passDesc.m_isDynamic = false;
		passDesc.m_subpassCount = 1;

		AttachmentDesc& attachDesc = passDesc.m_attachments[0];
		attachDesc.m_expectedState = PB::ETextureState::COLORTARGET;
		attachDesc.m_finalState = PB::ETextureState::COLORTARGET;
		attachDesc.m_format = Util::FormatToUnorm(m_renderer->GetSwapchain()->GetImageFormat());
		attachDesc.m_keepContents = true;
		attachDesc.m_loadAction = PB::EAttachmentAction::LOAD;

		SubpassDesc& subpassDesc = passDesc.m_subpasses[0];
		AttachmentUsageDesc& attachUsageDesc = subpassDesc.m_attachments[0];
		attachUsageDesc.m_attachmentFormat = attachDesc.m_format;
		attachUsageDesc.m_attachmentIdx = 0;
		attachUsageDesc.m_usage = PB::EAttachmentUsage::COLOR;

		return m_renderer->GetRenderPassCache()->GetRenderPass(passDesc);
	}
};