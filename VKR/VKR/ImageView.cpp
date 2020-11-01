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

	void ViewCache::Init(Device* device, VkDescriptorSet* outMasterSet, VkDescriptorSetLayout* outMasterSetLayout)
	{
		m_device = device;
		m_descriptorRegistry.Create(device, outMasterSet, outMasterSetLayout);
	}

	void ViewCache::Destroy()
	{
		for (auto& view : m_texViewCache)
			vkDestroyImageView(m_device->GetHandle(), view.second.m_view, nullptr);
		for (auto& sampler : m_samplerCache)
		{
			m_descriptorRegistry.FreeView(EDescriptorType::DESCRIPTORTYPE_SAMPLER, sampler.second.m_descriptorIndex);
			vkDestroySampler(m_device->GetHandle(), sampler.second.m_sampler, nullptr);
		}
		m_device = nullptr;
		m_descriptorRegistry.Destroy();
	}

	TextureView ViewCache::GetTextureView(const TextureViewDesc& desc)
	{
		PB_ASSERT_MSG(desc.m_expectedState != ETextureState::COLORTARGET, "Cannot use GetTextureView to get a render target view. Use GetRenderTargetView instead.");

		auto it = m_texViewCache.find(desc);
		if (it == m_texViewCache.end())
		{
			auto& viewData = m_texViewCache[desc];
			viewData = CreateTextureView(desc);
			return viewData.m_descriptorIndex;
		}
		else
			return it->second.m_descriptorIndex;
	}

	TextureView ViewCache::GetRenderTargetView(const TextureViewDesc& desc)
	{
		PB_ASSERT_MSG(desc.m_expectedState == ETextureState::COLORTARGET || desc.m_expectedState == ETextureState::DEPTHTARGET, "Cannot use GetRenderTargetView to get a non-render target view. Use GetTextureView instead.");

		auto it = m_texViewCache.find(desc);
		if (it == m_texViewCache.end())
		{
			auto& viewData = m_texViewCache[desc];
			viewData = CreateTextureView(desc);
			return reinterpret_cast<TextureView>(&viewData);
		}
		else
			return reinterpret_cast<TextureView>(&it->second);
	}

	void ViewCache::DestroyTextureView(const TextureViewDesc& desc)
	{
		// TODO: Reset descriptor at the view's index to avoid submitting views of destroyed resources.
		auto it = m_texViewCache.find(desc);
		bool cacheMiss = it == m_texViewCache.end();
		PB_ASSERT(cacheMiss == false);
		m_descriptorRegistry.FreeView(EDescriptorType::DESCRIPTORTYPE_TEXTURE, it->second.m_descriptorIndex);
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
		// TODO: Reset descriptor at the view's index to avoid submitting views of destroyed resources.
		auto it = m_bufViewCache.find(desc);
		bool cacheMiss = it == m_bufViewCache.end();
		PB_ASSERT(cacheMiss == false);
		m_bufViewCache.erase(it);
	}

	Sampler ViewCache::GetSampler(const SamplerDesc& desc)
	{
		auto it = m_samplerCache.find(desc);
		if (it == m_samplerCache.end())
		{
			auto& viewData = m_samplerCache[desc];
			viewData = CreateSampler(desc);
			return viewData.m_descriptorIndex;
		}
		else
			return it->second.m_descriptorIndex;
	}

	TextureViewData ViewCache::CreateTextureView(const TextureViewDesc& desc)
	{
		Texture* internalTex = reinterpret_cast<Texture*>(desc.m_texture);
		PB_ASSERT(internalTex->GetUsage() & desc.m_expectedState);

		bool hasDepthPlane = false;
		bool hasStencilPlane = false;
		switch (desc.m_format)
		{
		case ETextureFormat::D16_UNORM:
		case ETextureFormat::D32_FLOAT:
			hasDepthPlane = true;
			hasStencilPlane = false;
			break;
		case ETextureFormat::D16_UNORM_S8_UINT:
		case ETextureFormat::D24_UNORM_S8_UINT:
		case ETextureFormat::D32_FLOAT_S8_UINT:
			hasDepthPlane = true;
			hasStencilPlane = true;
			break;
		default:
			break;
		}

		VkImageViewCreateInfo imageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
		imageViewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		imageViewInfo.flags = 0;
		imageViewInfo.format = ConvertPBFormatToVkFormat(desc.m_format);
		imageViewInfo.image = internalTex->GetImage();
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		// TODO: Let the user select depth or stencil in the view desc.
		if(hasDepthPlane)
			imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		//if(hasStencilPlane)
		//	imageViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

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

		if(desc.m_expectedState == ETextureState::SAMPLED)
			m_descriptorRegistry.RegisterView(viewData);
		internalTex->RegisterView(desc);

		return viewData;
	}

	BufferViewData ViewCache::CreateBufferView(const BufferViewDesc& desc)
	{
		PB_ASSERT(reinterpret_cast<BufferObject*>(desc.m_buffer)->GetUsage() & EBufferUsage::UNIFORM);

		BufferViewData viewData;
		viewData.m_buffer = reinterpret_cast<BufferObject*>(desc.m_buffer)->GetHandle();
		viewData.m_offset = desc.m_offset;
		viewData.m_size = desc.m_size;
		reinterpret_cast<BufferObject*>(desc.m_buffer)->RegisterView(desc);
		return viewData;
	}

	inline VkFilter PBFilterToVKFilter(const ESamplerFilter& filter)
	{
		switch (filter)
		{
		case ESamplerFilter::NEAREST:
			return VK_FILTER_NEAREST;
			break;
		case ESamplerFilter::BILINEAR:
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
		case ESamplerRepeatMode::REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case ESamplerRepeatMode::MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			break;
		case ESamplerRepeatMode::CLAMP:
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
		case ESamplerFilter::NEAREST:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case ESamplerFilter::BILINEAR:
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
		samplerInfo.anisotropyEnable = desc.m_anisotropyLevels > 1.0f;
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