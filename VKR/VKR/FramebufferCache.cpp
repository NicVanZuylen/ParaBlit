#include "FramebufferCache.h"
#include "Device.h"
#include "ParaBlitDebug.h"

#include "vulkan/vulkan.h"
#include "MurmurHash3.h"

namespace PB
{
	bool FramebufferDesc::operator==(const FramebufferDesc& other) const
	{
		return m_renderPass == other.m_renderPass && m_width == other.m_width && m_height == other.m_height && m_attachmentCount == other.m_attachmentCount 
			&& std::memcmp(m_attachmentViews, other.m_attachmentViews, sizeof(TextureView) * m_attachmentCount) == 0;
	}

	size_t FramebufferDescHasher::operator()(const FramebufferDesc& desc) const
	{
		auto descHash = MurmurHash3_x64_64(&desc, static_cast<int>(sizeof(FramebufferDesc) - sizeof(FramebufferDesc::m_attachmentViews)), 0);
		return MurmurHash3_x64_64(desc.m_attachmentViews, static_cast<int>(sizeof(TextureView) * desc.m_attachmentCount), descHash);
	}

	void FramebufferCache::Init(Device* device)
	{
		m_device = device;
	}

	void FramebufferCache::Destroy()
	{
		for (auto& framebuffer : m_cache)
			vkDestroyFramebuffer(m_device->GetHandle(), reinterpret_cast<VkFramebuffer>(framebuffer.second), nullptr);
		m_device = nullptr;
	}

	Framebuffer FramebufferCache::GetFramebuffer(const FramebufferDesc& desc)
	{
		auto it = m_cache.find(desc);
		if (it == m_cache.end())
		{
			auto newFramebuffer = CreateFramebuffer(desc);
			m_cache[desc] = newFramebuffer;
			return newFramebuffer;
		}
		else
			return it->second;
	}

	Framebuffer FramebufferCache::CreateFramebuffer(const FramebufferDesc& desc)
	{
		VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr };
		framebufferInfo.width = desc.m_width;
		framebufferInfo.height = desc.m_height;
		framebufferInfo.flags = 0;
		framebufferInfo.layers = 1;
		framebufferInfo.renderPass = reinterpret_cast<VkRenderPass>(desc.m_renderPass);
		framebufferInfo.pAttachments = reinterpret_cast<VkImageView*>(desc.m_attachmentViews);
		framebufferInfo.attachmentCount = static_cast<u32>(desc.m_attachmentCount);

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreateFramebuffer(m_device->GetHandle(), &framebufferInfo, nullptr, &framebuffer));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(framebuffer);
		return framebuffer;
	}
}
