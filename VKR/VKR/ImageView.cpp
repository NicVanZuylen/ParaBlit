#include "ImageView.h"
#include "PBUtil.h"
#include "ParaBlitDebug.h"
#include "MurmurHash3.h"
#include "Renderer.h"
#include "Device.h"

namespace PB
{
	bool TextureViewDesc::operator==(const TextureViewDesc& other) const
	{
		return m_texture == other.m_texture && m_subresources == other.m_subresources;
	}

	size_t TextureViewDescHasher::operator()(const TextureViewDesc& desc) const
	{
		PB_STATIC_ASSERT(sizeof(TextureViewDesc) % 16 == 0, "TextureViewDesc does not meet optimal alignment requirements for hashing.");
		return MurmurHash3_x64_64(&desc, sizeof(TextureViewDesc), 0);
	}

	TextureViewCache::TextureViewCache()
	{
	}

	TextureViewCache::~TextureViewCache()
	{
	}

	void TextureViewCache::Init(Device* device)
	{
		m_device = device;
	}

	void TextureViewCache::Destroy()
	{
		for (auto& view : m_viewCache)
			vkDestroyImageView(m_device->GetHandle(), view.second.m_view, nullptr);
		m_device = nullptr;
	}

	TextureView TextureViewCache::GetView(const TextureViewDesc& desc)
	{
		auto it = m_viewCache.find(desc);
		if (it == m_viewCache.end())
		{
			auto view = CreateView(desc);
			m_viewCache[desc] = view;
			return view.m_view;
		}
		else
			return it->second.m_view;
	}

	ViewData TextureViewCache::CreateView(const TextureViewDesc& desc)
	{
		VkImageViewCreateInfo imageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
		imageViewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		imageViewInfo.flags = 0;
		imageViewInfo.format = ConvertPBFormatToVkFormat(desc.m_format);
		imageViewInfo.image = reinterpret_cast<Texture*>(desc.m_texture)->GetImage();
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // TODO: Allow for array views and in the future possibly 3D image views.

		VkImageView newView = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreateImageView(m_device->GetHandle(), &imageViewInfo, nullptr, &newView));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newView);

		return { newView };
	}
}