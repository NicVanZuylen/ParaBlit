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
		return std::memcmp(this, &other, sizeof(TextureViewDesc)) == 0;
	}

	bool BufferViewDesc::operator==(const BufferViewDesc& other) const
	{
		return std::memcmp(this, &other, sizeof(BufferViewDesc)) == 0;
	}

	bool SamplerDesc::operator==(const SamplerDesc& other) const
	{
		return std::memcmp(this, &other, sizeof(SamplerDesc)) == 0;
	}

	size_t TextureViewDescHasher::operator()(const TextureViewDesc& desc) const
	{
		PB_STATIC_ASSERT(sizeof(TextureViewDesc) % 16 == 0, "TextureViewDesc does not meet optimal alignment requirements for hashing.");
		return MurmurHash3_x64_64(&desc, sizeof(TextureViewDesc), 0);
	}

	size_t BufferViewDescHasher::operator()(const BufferViewDesc& desc) const
	{
		PB_STATIC_ASSERT(sizeof(BufferViewDesc) % 16 == 0, "BufferViewDesc does not meet optimal alignment requirements for hashing.");
		return MurmurHash3_x64_64(&desc, sizeof(BufferViewDesc), 0);
	}

	size_t SamplerDescHasher::operator()(const SamplerDesc& desc) const
	{
		PB_STATIC_ASSERT(sizeof(SamplerDesc) % 16 == 0, "SamplerDesc does not meet optimal alignment requirements for hashing.");
		return MurmurHash3_x64_64(&desc, sizeof(SamplerDesc), 0);
	}

	ViewCache::ViewCache()
	{
	}

	ViewCache::~ViewCache()
	{
	}

	void ViewCache::Init(Device* device)
	{
		m_device = device;
		m_descriptorRegistry.Create(device);
	}

	void ViewCache::Destroy()
	{
		for (auto& view : m_texViewCache)
			vkDestroyImageView(m_device->GetHandle(), view.second.m_view, nullptr);
		for (auto& sampler : m_samplerCache)
		{
			m_descriptorRegistry.FreeView(DESCRIPTORTYPE_SAMPLER, sampler.second.m_descriptorIndex);
			vkDestroySampler(m_device->GetHandle(), sampler.second.m_sampler, nullptr);
		}
		m_device = nullptr;
		m_descriptorRegistry.Destroy();
	}

	TextureView ViewCache::GetTextureView(const TextureViewDesc& desc)
	{
		auto it = m_texViewCache.find(desc);
		if (it == m_texViewCache.end())
		{
			auto& viewData = m_texViewCache[desc];
			viewData = CreateTextureView(desc);
			return &viewData;
		}
		else
			return &it->second;
	}

	void ViewCache::DestroyTextureView(const TextureViewDesc& desc)
	{
		auto it = m_texViewCache.find(desc);
		PB_ASSERT(it != m_texViewCache.end());
		m_descriptorRegistry.FreeView(DESCRIPTORTYPE_TEXTURE, it->second.m_descriptorIndex);
		vkDestroyImageView(m_device->GetHandle(), it->second.m_view, nullptr);
		m_texViewCache.erase(it);
	}

	BufferView ViewCache::GetBufferView(const BufferViewDesc& desc)
	{
		auto it = m_bufViewCache.find(desc);
		if (it == m_bufViewCache.end())
		{
			auto& viewData = m_bufViewCache[desc];
			viewData = CreateBufferView(desc);
			return &viewData;
		}
		else
			return &it->second;
	}

	void ViewCache::DestroyBufferView(const BufferViewDesc& desc)
	{
		auto it = m_bufViewCache.find(desc);
		PB_ASSERT(it != m_bufViewCache.end());
		m_descriptorRegistry.FreeView(DESCRIPTORTYPE_UNIFORM_BUFFER, it->second.m_descriptorIndex);
		m_bufViewCache.erase(it);
	}

	Sampler ViewCache::GetSampler(const SamplerDesc& desc)
	{
		auto it = m_samplerCache.find(desc);
		if (it == m_samplerCache.end())
		{
			auto& viewData = m_samplerCache[desc];
			viewData = CreateSampler(desc);
			return &viewData;
		}
		else
			return &it->second;
	}

	TextureViewData ViewCache::CreateTextureView(const TextureViewDesc& desc)
	{
		Texture* internalTex = reinterpret_cast<Texture*>(desc.m_texture);
		PB_ASSERT(internalTex->GetUsage() & desc.m_expectedState);

		VkImageViewCreateInfo imageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
		imageViewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		imageViewInfo.flags = 0;
		imageViewInfo.format = ConvertPBFormatToVkFormat(desc.m_format);
		imageViewInfo.image = internalTex->GetImage();
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

		TextureViewData viewData;
		viewData.m_view = newView;
		viewData.m_expectedState = desc.m_expectedState;

		if(desc.m_expectedState & (PB_TEXTURE_STATE_SAMPLED))
			m_descriptorRegistry.RegisterView(viewData);
		internalTex->RegisterView(desc);

		return viewData;
	}

	BufferViewData ViewCache::CreateBufferView(const BufferViewDesc& desc)
	{
		PB_ASSERT(reinterpret_cast<BufferObject*>(desc.m_buffer)->GetUsage() & PB::PB_BUFFER_USAGE_UNIFORM);

		BufferViewData viewData;
		viewData.m_buffer = reinterpret_cast<BufferObject*>(desc.m_buffer)->GetHandle();
		viewData.m_offset = desc.m_offset;
		viewData.m_size = desc.m_size;
		m_descriptorRegistry.RegisterView(viewData);
		return viewData;
	}

	inline VkFilter PBFilterToVKFilter(const ESamplerFilter& filter)
	{
		switch (filter)
		{
		case PB_SAMPLER_FILTER_NEAREST:
			return VK_FILTER_NEAREST;
			break;
		case PB_SAMPLER_FILTER_BILINEAR:
			return VK_FILTER_LINEAR;
			break;
		default:
			return VK_FILTER_NEAREST;
			break;
		}
	}

	inline VkSamplerAddressMode PBRepeatModeToVkSamplerAddressMode(const ESamplerRepeatMode& mode)
	{
		switch (mode)
		{
		case PB_SAMPLER_REPEAT_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case PB_SAMPLER_REPEAT_MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			break;
		case PB_SAMPLER_REPEAT_CLAMP:
			return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
			break;
		default:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		}
	}

	inline VkSamplerMipmapMode PBFilterToSamplerMipMode(const ESamplerFilter& filter)
	{
		switch (filter)
		{
		case PB_SAMPLER_FILTER_NEAREST:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case PB_SAMPLER_FILTER_BILINEAR:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		default:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		}
	}

	SamplerData ViewCache::CreateSampler(const SamplerDesc& desc)
	{
		VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr };
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.addressModeU = PBRepeatModeToVkSamplerAddressMode(desc.m_repeatMode);
		samplerInfo.addressModeV = PBRepeatModeToVkSamplerAddressMode(desc.m_repeatMode);
		samplerInfo.addressModeW = PBRepeatModeToVkSamplerAddressMode(desc.m_repeatMode);
		samplerInfo.anisotropyEnable = desc.m_anisotropyLevels > 0.0f;
		samplerInfo.maxAnisotropy = desc.m_anisotropyLevels;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.minFilter = PBFilterToVKFilter(desc.m_filter);
		samplerInfo.magFilter = PBFilterToVKFilter(desc.m_filter);
		samplerInfo.mipmapMode = PBFilterToSamplerMipMode(desc.m_mipFilter);
		samplerInfo.minLod = 0.0f;
		samplerInfo.mipLodBias = 0.0f;
		
		SamplerData data{};
		PB_ERROR_CHECK(vkCreateSampler(m_device->GetHandle(), &samplerInfo, nullptr, &data.m_sampler));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(data.m_sampler);

		m_descriptorRegistry.RegisterView(data);

		return data;
	}
}