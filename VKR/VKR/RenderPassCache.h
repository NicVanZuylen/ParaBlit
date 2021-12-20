#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"
#include "IRenderPassCache.h"
#include "Texture.h"

#include <unordered_map>

namespace PB 
{
	struct RenderPassHasher
	{
		// Hash function for the map.
		size_t operator()(const RenderPassDesc& desc) const;
	};

	class Device;

	class RenderPassCache : public IRenderPassCache
	{
	public:

		struct DynamicRenderPass
		{
			VkRenderingInfoKHR m_renderingInfo;
			CLib::Vector<VkRenderingAttachmentInfoKHR, 8> m_colorAttachmentInfos{};
			VkRenderingAttachmentInfoKHR m_depthAttachmentInfo;
			VkRenderingAttachmentInfoKHR m_stencilAttachmentInfo;
			CLib::Vector<VkFormat, 8> m_colorAttachmentFormats{};
			VkCommandBufferInheritanceRenderingInfoKHR m_inheritanceInfo{};
		};

		RenderPassCache();

		~RenderPassCache();

		PARABLIT_API void Init(Device* device);

		PARABLIT_API void Destroy();

		PARABLIT_API RenderPass GetRenderPass(const RenderPassDesc& desc) override;

	private:

		PARABLIT_API inline VkRenderPass CreateRenderPass(const RenderPassDesc& desc);
		PARABLIT_API inline DynamicRenderPass* CreateRenderPassDynamic(const RenderPassDesc& desc);

		std::unordered_map<RenderPassDesc, void*, RenderPassHasher> m_cache;
		Device* m_device = nullptr;
	};
}