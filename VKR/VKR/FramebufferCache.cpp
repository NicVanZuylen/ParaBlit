#include "FramebufferCache.h"
#include "Device.h"
#include "ParaBlitDebug.h"
#include "ImageView.h"
#include "CLib/Vector.h"
#include "MurmurHash3.h"

namespace PB
{
	bool FramebufferDesc::operator==(const FramebufferDesc& other) const
	{
		bool equal = m_renderPass == other.m_renderPass && m_width == other.m_width && m_height == other.m_height;
		if (equal)
		{
			for (u32 i = 0; i < _countof(m_attachmentViews); ++i)
			{
				equal &= (m_attachmentViews[i] == other.m_attachmentViews[i]);
			}
		}
		return equal;
	}

	size_t FramebufferDescHasher::operator()(const FramebufferDesc& desc) const
	{
		return MurmurHash3_x64_64(&desc, sizeof(FramebufferDesc), 0);
	}

	void FramebufferCache::Init(Device* device)
	{
		m_device = device;
	}

	void FramebufferCache::Destroy()
	{
		for (auto& framebuffer : m_cache)
			vkDestroyFramebuffer(m_device->GetHandle(), reinterpret_cast<VkFramebuffer>(framebuffer.second.m_framebuffer), nullptr);
		m_cache.clear();
		m_device = nullptr;
	}

	Framebuffer FramebufferCache::GetFramebuffer(const FramebufferDesc& desc)
	{
		auto it = m_cache.find(desc);
		if (it == m_cache.end())
		{
			FramebufferData& data = m_cache[desc];
			CreateFramebuffer(data, desc);
			return data.m_framebuffer;
		}
		else
		{
			auto& data = it->second;
			bool validFramebuffer = true;
			for (u32 i = 0; i < _countof(FramebufferDesc::m_attachmentViews); ++i)
			{
				u32 id = data.m_viewUniqueIds[i];
				if (desc.m_attachmentViews[i] == 0)
					break;

				u32 otherId = reinterpret_cast<TextureViewData*>(desc.m_attachmentViews[i])->m_uniqueId;
				validFramebuffer &= (id == otherId);
			}

			if (!validFramebuffer)
			{
				// This framebuffer probably references invalid views. So it should be re-created.
				vkDestroyFramebuffer(m_device->GetHandle(), reinterpret_cast<VkFramebuffer>(data.m_framebuffer), nullptr);
				data.m_framebuffer = nullptr;

				CreateFramebuffer(data, desc);
			}

			return data.m_framebuffer;
		}
	}

	void FramebufferCache::CreateFramebuffer(FramebufferData& outFramebufferData, const FramebufferDesc& desc)
	{
		CLib::Vector<VkImageView, 8> views;
		for (auto& view : desc.m_attachmentViews)
		{
			if (view == 0)
				break;

			auto* viewData = reinterpret_cast<TextureViewData*>(view);
			outFramebufferData.m_viewUniqueIds.PushBack(viewData->m_uniqueId);
			views.PushBack(viewData->m_view);
		}

		VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr };
		framebufferInfo.width = desc.m_width;
		framebufferInfo.height = desc.m_height;
		framebufferInfo.flags = 0;
		framebufferInfo.layers = 1;
		framebufferInfo.renderPass = reinterpret_cast<VkRenderPass>(desc.m_renderPass);
		framebufferInfo.pAttachments = views.Data();
		framebufferInfo.attachmentCount = views.Count();

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreateFramebuffer(m_device->GetHandle(), &framebufferInfo, nullptr, &framebuffer));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(framebuffer);
		
		outFramebufferData.m_framebuffer = framebuffer;
	}
}
