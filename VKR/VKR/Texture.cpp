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
		m_availableStates = desc.m_usageStates;
		m_extents = { desc.m_width, desc.m_height, 1 };

		auto& dataDesc = desc.m_data;
		PB_ASSERT(dataDesc.m_format != PB_TEXTURE_FORMAT_UNKNOWN);

		m_format = dataDesc.m_format;
		m_isAlias = dataDesc.m_aliasOther;

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
		m_currentState = desc.m_currentUsage;
		m_availableStates = desc.m_usageFlags;
		m_ownsImage = false;
		m_hasDepthPlane = false;
		m_hasStencilPlane = false;
		m_isAlias = false;

		PB_ASSERT(((m_availableStates & m_currentState) || m_currentState == PB_TEXTURE_STATE_NONE));
	}

	void Texture::Destroy()
	{
		if (m_image)
		{
			for (auto& viewDesc : m_viewDescs)
				m_renderer->GetViewCache()->DestroyTextureView(viewDesc);

			if (m_ownsImage)
			{
				vkDestroyImage(m_device->GetHandle(), m_image, nullptr);
				m_image = VK_NULL_HANDLE;

				// Free memory block.
				if(!m_isAlias)
					m_device->GetDeviceAllocator().Free(m_memoryBlock);

				m_currentState = PB_TEXTURE_STATE_NONE;
				m_availableStates = PB_TEXTURE_STATE_NONE;
				m_format = PB_TEXTURE_FORMAT_UNKNOWN;
				m_ownsImage = true;
			}
		}
	}

	VkImage Texture::GetImage()
	{
		return m_image;
	}

	void Texture::SetState(ETextureState state)
	{
		m_currentState = state;
	}

	ETextureStateFlags Texture::GetUsage()
	{
		return m_availableStates;
	}

	ETextureState Texture::GetState()
	{
		return m_currentState;
	}

	bool Texture::CanAlias(ITexture* baseTexture)
	{
		if (!m_isAlias)
			return false;

		Texture* baseInternal = reinterpret_cast<Texture*>(baseTexture);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device->GetHandle(), m_image, &memRequirements);

		auto baseMemBlock = baseInternal->m_memoryBlock;
		return baseMemBlock.m_size >= memRequirements.size && baseMemBlock.m_start % memRequirements.alignment == 0 && baseMemBlock.m_memoryTypeIndex == m_device->FindMemoryTypeIndex(memRequirements.memoryTypeBits, baseMemBlock.m_memoryType);
	}

	void Texture::AliasTexture(ITexture* baseTexture)
	{
		PB_ASSERT_MSG(CanAlias(baseTexture), "This texture cannot alias the provided base texture.");

		m_memoryBlock = reinterpret_cast<Texture*>(baseTexture)->m_memoryBlock;

		// Bind base texture memory to this alias.
		PB_ERROR_CHECK(vkBindImageMemory(m_device->GetHandle(), m_image, m_memoryBlock.m_memory, m_memoryBlock.AlignedOffset()));
		PB_BREAK_ON_ERROR;
	}

	TextureView Texture::GetDefaultSRV()
	{
		PB_ASSERT_MSG(m_format != PB_TEXTURE_FORMAT_UNKNOWN, "Cannot get a default view of a texture with unknown format.")

		TextureViewDesc defaultSRVDesc;
		defaultSRVDesc.m_expectedState = PB_TEXTURE_STATE_SAMPLED;
		defaultSRVDesc.m_format = m_format;
		defaultSRVDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetTextureView(defaultSRVDesc);
	}

	TextureView Texture::GetDefaultRTV()
	{
		PB_ASSERT_MSG(m_format != PB_TEXTURE_FORMAT_UNKNOWN, "Cannot get a default view of a texture with unknown format.")

		TextureViewDesc defaultRTVDesc;
		defaultRTVDesc.m_expectedState = m_hasDepthPlane ? PB_TEXTURE_STATE_DEPTHTARGET : PB_TEXTURE_STATE_COLORTARGET;
		defaultRTVDesc.m_format = m_format;
		defaultRTVDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetRenderTargetView(defaultRTVDesc);
	}

	TextureView Texture::GetView(TextureViewDesc& viewDesc)
	{
		viewDesc.m_texture = this;
		return m_renderer->GetViewCache()->GetTextureView(viewDesc);
	}

	TextureView Texture::GetRenderTargetView(TextureViewDesc& viewDesc)
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

	bool Texture::HasDepthPlane()
	{
		return m_hasDepthPlane;
	}

	bool Texture::HasStencilPlane()
	{
		return m_hasStencilPlane;
	}

	bool Texture::CreateImageResource(const TextureDesc& desc)
	{
		PB_ASSERT_MSG(desc.m_usageStates > 0, "No usage flags provided.");
		PB_ASSERT_MSG((desc.m_usageStates & PB_TEXTURE_STATE_MAX) == 0, "Invalid texture usage flag provided.");
		PB_ASSERT_MSG(desc.m_width > 0 && desc.m_height > 0, "Texture cannot be zero width or height.");

		if (desc.m_usageStates == 0 || (desc.m_usageStates & PB_TEXTURE_STATE_MAX) > 0)
			return false;

		switch (desc.m_data.m_format)
		{
		case PB_TEXTURE_FORMAT_D16_UNORM:
		case PB_TEXTURE_FORMAT_D32_FLOAT:
			m_hasDepthPlane = true;
			m_hasStencilPlane = false;
			break;
		case PB_TEXTURE_FORMAT_D16_UNORM_S8_UINT:
		case PB_TEXTURE_FORMAT_D24_UNORM_S8_UINT:
		case PB_TEXTURE_FORMAT_D32_FLOAT_S8_UINT:
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
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = { desc.m_width, desc.m_height, 1U };
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		auto queueFamilyIndex = static_cast<uint32_t>(m_renderer->GetDevice()->GetGraphicsQueueFamilyIndex());
		imageInfo.pQueueFamilyIndices = &queueFamilyIndex;
		imageInfo.queueFamilyIndexCount = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

		// Determine image usage.
		if (((desc.m_initOptions & PB_TEXTURE_INIT_USE_DATA) || (desc.m_initOptions & PB_TEXTURE_INIT_ZERO_INITIALIZE)))
			m_availableStates |= PB_TEXTURE_STATE_COPY_DST;

		imageInfo.usage = ConvertPBAvailableStatesToUsageFlags(m_availableStates);

		PB_ASSERT(m_image == VK_NULL_HANDLE); // Make sure we're not leaking the previous image if re-created.
		PB_ERROR_CHECK(vkCreateImage(m_device->GetHandle(), &imageInfo, nullptr, &m_image));
		PB_ASSERT(m_image);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device->GetHandle(), m_image, &memRequirements);

		if (!m_isAlias)
		{
			PB_ASSERT_MSG(AllocateMemory(desc, memRequirements), "Failed to allocate memory for image.");

			// Bind memory block to this texture's image.
			PB_ERROR_CHECK(vkBindImageMemory(m_device->GetHandle(), m_image, m_memoryBlock.m_memory, m_memoryBlock.AlignedOffset()));
			PB_BREAK_ON_ERROR;
		}

		return true;
	}

	bool Texture::AllocateMemory(const TextureDesc& desc, const VkMemoryRequirements& memRequirements)
	{
		m_memoryBlock = m_device->GetDeviceAllocator().Alloc(memRequirements, PB_MEMORY_TYPE_DEVICE_LOCAL, static_cast<u32>(memRequirements.size));
		if (!m_memoryBlock.m_memory)
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

		PB_ASSERT_MSG(!((desc.m_initOptions & PB_TEXTURE_INIT_USE_DATA) && (desc.m_initOptions & PB_TEXTURE_INIT_ZERO_INITIALIZE)), "Incompatible texture initialization flags provided: PB_TEXTURE_INIT_USE_DATA, and PB_TEXTURE_INIT_ZERO_INITIALIZE.");

		if (desc.m_initOptions & PB_TEXTURE_INIT_ZERO_INITIALIZE)
		{
			PB_ASSERT_MSG(m_availableStates & PB_TEXTURE_STATE_COPY_DST, "Cannot zero initialize an image which does not support copy dst.");

			// TODO: Share an internal context for initialization of all resources, as making a context for every resource will quickly bloat the graphics queue with many contexts in complex scenes.
			CommandContext internalContext;
			MakeInternalContext(internalContext, m_renderer);
			internalContext.Begin();

			if (m_currentState != PB_TEXTURE_STATE_COPY_DST || m_currentState != PB_TEXTURE_STATE_RAW)
			{
				SubresourceRange pbSubresourceRange;
				pbSubresourceRange.m_baseMip = 0; // TODO: Support for subresources.
				pbSubresourceRange.m_mipCount = 1;
				pbSubresourceRange.m_firstArrayElement = 0;
				pbSubresourceRange.m_arrayCount = 1;
				internalContext.CmdTransitionTexture(this, PB_TEXTURE_STATE_COPY_DST, pbSubresourceRange);
			}

			// Clear image to zero by clearing it with black.
			VkImageSubresourceRange subresources;
			subresources.baseMipLevel = 0;
			subresources.baseArrayLayer = 0;
			subresources.layerCount = 1;
			subresources.levelCount = 1;
			if (m_hasDepthPlane)
			{
				VkClearDepthStencilValue depthClear = { 0.0f, 1 };
				subresources.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				if (m_hasStencilPlane)
					subresources.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				vkCmdClearDepthStencilImage(internalContext.GetCmdBuffer(), m_image, ConvertPBStateToImageLayout(m_currentState), &depthClear, 1, &subresources);
			}
			else
			{
				VkClearColorValue clearColor = { 0, 0, 0, 0 };
				subresources.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				vkCmdClearColorImage(internalContext.GetCmdBuffer(), m_image, ConvertPBStateToImageLayout(m_currentState), &clearColor, 1, &subresources);
			}
			
			internalContext.End();
			internalContext.Return();
		}
		else if (desc.m_initOptions & PB_TEXTURE_INIT_USE_DATA)
		{
			auto stagingBuffer = m_device->GetTempBufferAllocator().NewTempBuffer(desc.m_data.m_size, m_renderer->GetCurrentFrame());
			auto* ptr = stagingBuffer.Map(m_device->GetHandle());

			memcpy(ptr, desc.m_data.m_data, desc.m_data.m_size);

			stagingBuffer.Unmap(m_device->GetHandle());

			PB::CommandContext internalContext;
			MakeInternalContext(internalContext, m_renderer);
			internalContext.Begin();

			PB::SubresourceRange subresources{};
			internalContext.CmdTransitionTexture(this, PB_TEXTURE_STATE_COPY_DST, subresources);

			VkBufferImageCopy region;
			region.bufferOffset = stagingBuffer.m_offset;
			region.bufferRowLength = desc.m_width;
			region.bufferImageHeight = desc.m_height;
			region.imageExtent = { desc.m_width, desc.m_height, 1 };
			region.imageOffset = { 0, 0, 0 };
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkCmdCopyBufferToImage(internalContext.GetCmdBuffer(), stagingBuffer.m_parentBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			internalContext.End();
			internalContext.Return();
		}
	}
}