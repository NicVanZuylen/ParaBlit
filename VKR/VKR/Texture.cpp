#include "Texture.h"
#include "ParaBlitDebug.h"
#include "Renderer.h"
#include "Device.h"
#include "CommandContext.h"
#include "PBUtil.h"

namespace PB 
{
	Texture::Texture()
	{
		m_ownsImage = false;
		m_hasDepthPlane = false;
		m_hasStencilPlane = false;
		m_isAlias = false;
	}

	Texture::~Texture()
	{
		Destroy();
	}

	void Texture::Create(IRenderer* renderer, const TextureDesc& desc)
	{
		Destroy();

		m_renderer = reinterpret_cast<Renderer*>(renderer);
		m_device = m_renderer->GetDevice();
		m_memoryType = desc.m_memoryType;
		m_availableStates = desc.m_usageStates;
		m_extents = { desc.m_width, desc.m_height, desc.m_depth };

		PB_ASSERT(desc.m_format != ETextureFormat::UNKNOWN);
		m_format = desc.m_format;
		m_isAlias = desc.m_aliasOther;

		if (!CreateImageResource(desc))
		{
			PB_LOG("Failed to create texture resource.");
			return;
		}
		m_ownsImage = true;

		InitializeMemory(desc);
	}

	void Texture::Create(Renderer* renderer, WrappedTextureDesc desc)
	{
		Destroy();

		m_renderer = renderer;
		m_device = m_renderer->GetDevice();
		m_image = desc.m_wrappedImage;
		m_extents = { desc.m_width, desc.m_height, 1 };
		m_memoryType = EMemoryType::DEVICE_LOCAL;
		m_availableStates = desc.m_usageFlags;
		m_format = desc.m_format;
		m_ownsImage = false;
		m_hasDepthPlane = false;
		m_hasStencilPlane = false;
		m_isAlias = false;
		m_isMapped = false;
	}

	void Texture::Destroy()
	{
		PB_ASSERT_MSG(m_isMapped == false, "Attempting to destroy a currently mapped resource.");

		if (m_image)
		{
			for (auto& viewDesc : m_viewDescs)
				m_renderer->GetViewCache()->DestroyTextureView(viewDesc);

			if (m_ownsImage)
			{
				vkDestroyImage(m_device->GetHandle(), m_image, nullptr);
				m_image = VK_NULL_HANDLE;

				// Free memory block.
				if (!m_isAlias)
				{
					m_device->GetTextureAllocator(m_memoryType).Free(m_poolAllocation);
				}

				m_availableStates = ETextureState::NONE;
				m_format = ETextureFormat::UNKNOWN;
				m_ownsImage = true;
			}
		}
	}

	VkImage Texture::GetImage()
	{
		return m_image;
	}

	TextureStateFlags Texture::GetUsage()
	{
		return m_availableStates;
	}

	bool Texture::CanAlias(ITexture* baseTexture)
	{
		if (!m_isAlias)
			return false;

		Texture* baseInternal = reinterpret_cast<Texture*>(baseTexture);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device->GetHandle(), m_image, &memRequirements);

		auto basePoolAllocation = baseInternal->m_poolAllocation;
		bool validSize = basePoolAllocation.m_size >= memRequirements.size;
		bool validAlignment = (basePoolAllocation.m_offset % memRequirements.alignment) == 0;
		return (validSize && validAlignment);
	}

	void Texture::AliasTexture(ITexture* baseTexture)
	{
		PB_ASSERT_MSG(CanAlias(baseTexture), "This texture cannot alias the provided base texture.");

		m_poolAllocation = reinterpret_cast<Texture*>(baseTexture)->m_poolAllocation;

		// Bind base texture memory to this alias.
		PB_ERROR_CHECK(vkBindImageMemory(m_device->GetHandle(), m_image, m_poolAllocation.m_memoryHandle, m_poolAllocation.m_offset));
		PB_BREAK_ON_ERROR;
	}

	void Texture::GetMemorySizeAndAlign(u32& outSizeBytes, u32& outAlignBytes)
	{
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device->GetHandle(), m_image, &memRequirements);

		outSizeBytes = u32(memRequirements.size);
		outAlignBytes = u32(memRequirements.alignment);
	}

	u8* Texture::MapReadback()
	{
		PB_ASSERT(m_isMapped == false);
		PB_ASSERT_MSG(m_memoryType == EMemoryType::HOST_VISIBLE, "Resource is not allocated in host-visible memory.");
		PB_ASSERT_MSG(m_mipCount * m_arrayLayerCount == 1, "Readback resources should only have a single subresource.");

		VkImageSubresource subresource;
		subresource.arrayLayer = 0;
		subresource.mipLevel = 0;
		subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkSubresourceLayout subresourceLayout;
		vkGetImageSubresourceLayout(m_device->GetHandle(), m_image, &subresource, &subresourceLayout);

		VkMappedMemoryRange memoryRange{};
		memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memoryRange.pNext = nullptr;
		memoryRange.memory = m_poolAllocation.m_memoryHandle;
		memoryRange.offset = m_poolAllocation.m_offset;
		memoryRange.size = m_poolAllocation.m_size;

		// Round down offset to a multiple of VkPhysicalDeviceLimits::nonCoherentAtomSize - as required by the Vulkan spec.
		// This should still flush the desired range of memory, just with a little excess.
		VkDeviceSize offsetAlign = m_renderer->GetDevice()->GetDeviceLimits()->nonCoherentAtomSize;
		memoryRange.offset += (offsetAlign - (memoryRange.offset % offsetAlign));
		memoryRange.offset -= offsetAlign;

		vkInvalidateMappedMemoryRanges(m_device->GetHandle(), 1, &memoryRange);

		m_isMapped = true;
		return reinterpret_cast<u8*>(m_poolAllocation.m_ptr) + subresourceLayout.offset + m_poolAllocation.m_offset;
	}

	void Texture::UnmapReadback()
	{
		PB_ASSERT(m_isMapped == true);
		PB_ASSERT_MSG(m_memoryType == EMemoryType::HOST_VISIBLE, "Resource is not allocated in host-visible memory.");
		PB_ASSERT_MSG(m_mipCount * m_arrayLayerCount == 1, "Readback resources should only have a single subresource.");

		m_isMapped = false;
	}

	ResourceView Texture::GetDefaultSRV()
	{
		PB_ASSERT_MSG(m_format != ETextureFormat::UNKNOWN, "Cannot get a default view of a texture with unknown format.");

		TextureViewDesc defaultSRVDesc;
		defaultSRVDesc.m_expectedState = ETextureState::SAMPLED;
		defaultSRVDesc.m_format = m_format;
		defaultSRVDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetTextureView(defaultSRVDesc);
	}

	RenderTargetView Texture::GetDefaultRTV()
	{
		PB_ASSERT_MSG(m_format != ETextureFormat::UNKNOWN, "Cannot get a default view of a texture with unknown format.");

		TextureViewDesc defaultRTVDesc;
		defaultRTVDesc.m_expectedState = m_hasDepthPlane ? ETextureState::DEPTHTARGET : ETextureState::COLORTARGET;
		defaultRTVDesc.m_format = m_format;
		defaultRTVDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetRenderTargetView(defaultRTVDesc);
	}

	ResourceView Texture::GetView(TextureViewDesc& viewDesc)
	{
		viewDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetTextureView(viewDesc);
	}

	ResourceView Texture::GetDefaultSIV()
	{
		TextureViewDesc defaultSIVDesc;
		defaultSIVDesc.m_expectedState = ETextureState::STORAGE;
		defaultSIVDesc.m_format = m_format;
		defaultSIVDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetTextureView(defaultSIVDesc);
	}

	ResourceView Texture::GetViewAsStorageImage(TextureViewDesc& viewDesc)
	{
		viewDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetTextureView(viewDesc);
	}

	RenderTargetView Texture::GetRenderTargetView(TextureViewDesc& viewDesc)
	{
		viewDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetRenderTargetView(viewDesc);
	}

	VkExtent3D Texture::GetExtent()
	{
		return m_extents;
	}

	void Texture::RegisterView(const TextureViewDesc& desc)
	{
		m_viewDescs.PushBack() = desc;
	}

	bool Texture::CreateImageResource(const TextureDesc& desc)
	{
		PB_ASSERT_MSG(desc.m_usageStates != ETextureState::NONE, "No usage flags provided.");
		PB_ASSERT_MSG((desc.m_usageStates & ETextureState::MAX) == 0, "Invalid texture usage flag provided.");
		PB_ASSERT_MSG(desc.m_width > 0 && desc.m_height > 0, "Texture cannot be zero width or height.");
		PB_ASSERT_MSG(desc.m_mipCount >= 1, "Texture should have at least one mip level.");
		PB_ASSERT_MSG(desc.m_arraySize >= 1, "Texture should have at least one array layer.");
		PB_ASSERT_MSG((desc.m_usageStates & PB::ETextureState::READBACK) == 0 || (desc.m_mipCount * desc.m_arraySize) == 1, "Readback resources cannot have multiple subresources.");

		if (desc.m_usageStates == ETextureState::NONE || (desc.m_usageStates & ETextureState::MAX) > 0)
			return false;

		switch (desc.m_format)
		{
		case ETextureFormat::D16_UNORM:
		case ETextureFormat::D32_FLOAT:
			m_hasDepthPlane = true;
			m_hasStencilPlane = false;
			break;
		case ETextureFormat::D16_UNORM_S8_UINT:
		case ETextureFormat::D24_UNORM_S8_UINT:
		case ETextureFormat::D32_FLOAT_S8_UINT:
			m_hasDepthPlane = true;
			m_hasStencilPlane = true;
			break;
		default:
			m_hasDepthPlane = false;
			m_hasStencilPlane = false;
			break;
		}

		VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr };
		imageInfo.format = ConvertPBFormatToVkFormat(m_format);
		imageInfo.flags = 0;
		imageInfo.imageType = PBImageDimensionToVKImageType(desc.m_dimension);
		imageInfo.extent = { desc.m_width, desc.m_height, desc.m_depth };
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.mipLevels = m_mipCount = desc.m_mipCount;
		imageInfo.arrayLayers = m_arrayLayerCount = desc.m_arraySize; // Cube maps require an array layer for each face.
		auto queueFamilyIndex = static_cast<uint32_t>(m_renderer->GetDevice()->GetGraphicsQueueFamilyIndex());
		imageInfo.pQueueFamilyIndices = &queueFamilyIndex;
		imageInfo.queueFamilyIndexCount = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.tiling = (desc.m_usageStates & PB::ETextureState::READBACK) > 0 ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

		if (desc.m_dimension == ETextureDimension::DIMENSION_CUBE)
		{
			imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			imageInfo.arrayLayers = m_arrayLayerCount = 6;
		}

		// Determine image usage.
		if (((desc.m_initOptions & ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA) || (desc.m_initOptions & ETextureInitOptions::PB_TEXTURE_INIT_ZERO_INITIALIZE)))
			m_availableStates |= ETextureState::COPY_DST;

		// Readback should add COPY_DST to the available usage states alongside requesting a linear memory layout.
		if (desc.m_usageStates & PB::ETextureState::READBACK)
			m_availableStates |= ETextureState::COPY_DST;

		PB_ASSERT_MSG(((desc.m_initOptions & PB::ETextureInitOptions::PB_TEXTURE_INIT_GEN_MIPMAPS) == 0) || (desc.m_initOptions & PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA),
			"Mipmaps cannot be generated for a texture without source data.");
		if ((desc.m_initOptions & ETextureInitOptions::PB_TEXTURE_INIT_GEN_MIPMAPS) && desc.m_mipCount > 1)
			m_availableStates |= ETextureState::COPY_SRC;

		imageInfo.usage = ConvertPBAvailableStatesToUsageFlags(m_availableStates);

		PB_ASSERT(m_image == VK_NULL_HANDLE); // Make sure we're not leaking the previous image if re-created.
		PB_ERROR_CHECK(vkCreateImage(m_device->GetHandle(), &imageInfo, nullptr, &m_image));
		PB_ASSERT(m_image);

#if PB_USE_DEBUG_UTILS
		if (desc.m_name)
		{
			m_renderer->RegisterObjectName(desc.m_name, uint64_t(m_image), VK_OBJECT_TYPE_IMAGE);
		}
#endif

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device->GetHandle(), m_image, &memRequirements);

		if (!m_isAlias)
		{
			PB_ASSERT_MSG(AllocateMemory(desc, memRequirements), "Failed to allocate memory for image.");

			// Bind memory block to this texture's image.
			PB_ERROR_CHECK(vkBindImageMemory(m_device->GetHandle(), m_image, m_poolAllocation.m_memoryHandle, m_poolAllocation.m_offset));
			PB_BREAK_ON_ERROR;
		}

		return true;
	}

	bool Texture::AllocateMemory(const TextureDesc& desc, const VkMemoryRequirements& memRequirements)
	{
		PB_ASSERT(memRequirements.size + memRequirements.alignment <= ~uint32_t(0));
		m_device->GetTextureAllocator(m_memoryType).Alloc(uint32_t(memRequirements.size), uint32_t(memRequirements.alignment), m_poolAllocation);

		if (!m_poolAllocation.m_memoryHandle)
		{
			PB_ASSERT_MSG(false, "Failed to allocate texture memory block.");
			return false;
		}

		return true;
	}

	void Texture::InitializeMemory(const TextureDesc& desc)
	{
		if (m_isAlias)
			return;

		PB_ASSERT_MSG(!((desc.m_initOptions & ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA) && (desc.m_initOptions & ETextureInitOptions::PB_TEXTURE_INIT_ZERO_INITIALIZE)), "Incompatible texture initialization flags provided: PB_TEXTURE_INIT_USE_DATA, and PB_TEXTURE_INIT_ZERO_INITIALIZE.");

		if (desc.m_initOptions & ETextureInitOptions::PB_TEXTURE_INIT_ZERO_INITIALIZE)
		{
			PB_ASSERT_MSG(m_availableStates & ETextureState::COPY_DST, "Cannot zero initialize an image which does not support copy dst.");

			// TODO: Share an internal context for initialization of all resources, as making a context for every resource will quickly bloat the graphics queue with many contexts in complex scenes.
			CommandContext internalContext;
			MakeInternalContext(internalContext, m_renderer);
			internalContext.Begin();

			SubresourceRange pbSubresourceRange;
			pbSubresourceRange.m_baseMip = 0; // TODO: Support for subresources.
			pbSubresourceRange.m_mipCount = m_mipCount;
			pbSubresourceRange.m_firstArrayElement = 0;
			pbSubresourceRange.m_arrayCount = 1;
			internalContext.CmdTransitionTexture(this, ETextureState::NONE, ETextureState::COPY_DST, pbSubresourceRange);

			// Clear image to zero by clearing it with black.
			VkImageSubresourceRange subresources;
			subresources.baseMipLevel = 0;
			subresources.baseArrayLayer = 0;
			subresources.layerCount = 1;
			subresources.levelCount = m_mipCount;
			if (m_hasDepthPlane)
			{
				VkClearDepthStencilValue depthClear = { 0.0f, 1 };
				subresources.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				if (m_hasStencilPlane)
					subresources.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				vkCmdClearDepthStencilImage(internalContext.GetCmdBuffer(), m_image, ConvertPBStateToImageLayout(ETextureState::COPY_DST), &depthClear, 1, &subresources);
			}
			else
			{
				VkClearColorValue clearColor = { 0, 0, 0, 0 };
				subresources.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				vkCmdClearColorImage(internalContext.GetCmdBuffer(), m_image, ConvertPBStateToImageLayout(ETextureState::COPY_DST), &clearColor, 1, &subresources);
			}

			internalContext.End();
			internalContext.Return();
		}
		else if (desc.m_initOptions & ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA)
		{
			PB_ASSERT_MSG(desc.m_data != nullptr, "Texture is initialized using data, but no data is provided.");

			u32 totalDataSizeBytes = 0;
			u32 highestMip = 0;
			TextureDataDesc* currentDataDesc = desc.m_data;
			while (currentDataDesc)
			{
				totalDataSizeBytes += currentDataDesc->m_size;

				PB_ASSERT(currentDataDesc->m_data != nullptr);
				PB_ASSERT(currentDataDesc->m_size > 0);
				PB_ASSERT(currentDataDesc->m_mipLevel < desc.m_mipCount);
				PB_ASSERT(currentDataDesc->m_arrayLayer < (desc.m_dimension == ETextureDimension::DIMENSION_CUBE ? 6 : 1));

				if (currentDataDesc->m_mipLevel > highestMip)
					highestMip = currentDataDesc->m_mipLevel;

				currentDataDesc = currentDataDesc->m_next;
			}

			PB::CommandContext internalContext;
			MakeInternalContext(internalContext, m_renderer);
			internalContext.Begin();

			PB::SubresourceRange subresources{};
			subresources.m_mipCount = desc.m_mipCount;
			subresources.m_arrayCount = desc.m_dimension == ETextureDimension::DIMENSION_CUBE ? 6 : desc.m_arraySize;
			internalContext.CmdTransitionTexture(this, ETextureState::NONE, ETextureState::COPY_DST, subresources);

			auto stagingBuffer = m_device->GetTempBufferAllocator().NewTempBuffer(totalDataSizeBytes, m_renderer->GetCurrentSwapchainImageIndex(), PB::EMemoryType::HOST_VISIBLE, 1024);
			u8* dst = stagingBuffer.Start();
			u32 dstLocation = stagingBuffer.m_offset;
			CLib::Vector<VkBufferImageCopy, 8> dataDescs{};
			currentDataDesc = desc.m_data;
			while (currentDataDesc)
			{
				memcpy(dst, currentDataDesc->m_data, currentDataDesc->m_size);

				uint32_t mipLevel = currentDataDesc->m_mipLevel;
				uint32_t subresourceWidth = desc.m_width >> mipLevel;
				uint32_t subresourceHeight = desc.m_height >> mipLevel;

				VkBufferImageCopy& region = dataDescs.PushBack();
				region.bufferOffset = dstLocation;
				region.bufferRowLength = subresourceWidth;
				region.bufferImageHeight = subresourceHeight;
				region.imageExtent = { subresourceWidth, subresourceHeight, 1 };
				region.imageOffset = { 0, 0, 0 };
				region.imageSubresource.layerCount = 1;
				region.imageSubresource.baseArrayLayer = currentDataDesc->m_arrayLayer;
				region.imageSubresource.mipLevel = mipLevel;
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

				dst += currentDataDesc->m_size;
				dstLocation += currentDataDesc->m_size;
				currentDataDesc = currentDataDesc->m_next;
			}
			vkCmdCopyBufferToImage(internalContext.GetCmdBuffer(), stagingBuffer.m_buffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dataDescs.Count(), dataDescs.Data());

			if (highestMip == 0 && (desc.m_initOptions & ETextureInitOptions::PB_TEXTURE_INIT_GEN_MIPMAPS))
			{ 
				CLib::Vector<VkImageBlit, 8> blitOps{ subresources.m_arrayCount * (uint32_t(desc.m_mipCount - 1)) };
				for (uint32_t layer = 0; layer < subresources.m_arrayCount; ++layer)
				{
					PB::SubresourceRange srcSubresource{};
					srcSubresource.m_firstArrayElement = layer;
					srcSubresource.m_baseMip = 0;
					internalContext.CmdTransitionTexture(this, ETextureState::COPY_DST, ETextureState::COPY_SRC, srcSubresource);

					int mipWidth = int(desc.m_width / 2);
					int mipHeight = int(desc.m_height / 2);
					for (uint32_t mip = 1; mip < desc.m_mipCount; ++mip)
					{
						VkImageBlit& blit = blitOps.PushBack();

						blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						blit.srcSubresource.baseArrayLayer = layer;
						blit.srcSubresource.layerCount = 1;
						blit.srcSubresource.mipLevel = 0;
						blit.srcOffsets[0] = { 0, 0, 0 };
						blit.srcOffsets[1] = { int(desc.m_width), int(desc.m_height), 1 };

						blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						blit.dstSubresource.baseArrayLayer = layer;
						blit.dstSubresource.layerCount = 1;
						blit.dstSubresource.mipLevel = mip;
						blit.dstOffsets[0] = { 0, 0, 0 };
						blit.dstOffsets[1] = { mipWidth, mipHeight, 1 };

						mipWidth /= 2;
						mipHeight /= 2;
					}
				}
				vkCmdBlitImage(internalContext.GetCmdBuffer(), m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blitOps.Count() , blitOps.Data(), VK_FILTER_LINEAR);
			}

			internalContext.End();
			internalContext.Return();
		}
	}
}