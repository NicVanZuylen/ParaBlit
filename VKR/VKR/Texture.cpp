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

		auto& dataDesc = desc.m_data;
		PB_ASSERT(dataDesc.m_format != PB_TEXTURE_FORMAT_UNKNOWN);

		m_format = dataDesc.m_format;

		if (!CreateImageResource(desc))
		{
			PB_LOG("Failed to create texture resource.");
			return;
		}

		InitializeMemory(desc);
	}

	void Texture::Create(Renderer* renderer, WrappedTextureDesc desc)
	{
		Destroy();

		m_renderer = renderer;
		m_device = m_renderer->GetDevice();
		m_image = desc.m_wrappedImage;
		m_currentState = desc.m_currentUsage;
		m_availableStates = desc.m_usageFlags;
		m_ownsImage = false;

		PB_ASSERT(((m_availableStates & m_currentState) || m_currentState == PB_TEXTURE_STATE_NONE));
	}

	void Texture::Destroy()
	{
		if (m_ownsImage && m_image)
		{
			vkDestroyImage(m_device->GetHandle(), m_image, nullptr);
			m_image = VK_NULL_HANDLE;

			// Free memory block.
			m_device->GetDeviceAllocator().Free(m_memoryBlock);

			m_currentState = PB_TEXTURE_STATE_NONE;
			m_availableStates = PB_TEXTURE_STATE_NONE;
			m_format = PB_TEXTURE_FORMAT_UNKNOWN;
			m_ownsImage = true;
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

	bool Texture::CreateImageResource(const TextureDesc& desc)
	{
		PB_ASSERT(desc.m_usageStates > 0, "No usage flags provided.");
		PB_ASSERT((desc.m_usageStates & PB_TEXTURE_STATE_MAX) == 0, "Invalid texture usage flag provided.");
		PB_ASSERT(desc.m_width > 0 && desc.m_height > 0, "Texture cannot be zero width or height.");

		if (desc.m_usageStates == 0 || (desc.m_usageStates & PB_TEXTURE_STATE_MAX) > 0)
			return false;

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

		PB_ASSERT(AllocateMemory(desc, memRequirements), "Failed to allocate memory for image.");

		// Bind memory block to this texture's image.
		PB_ERROR_CHECK(vkBindImageMemory(m_device->GetHandle(), m_image, m_memoryBlock.m_memory, m_memoryBlock.AlignedOffset()));
		PB_BREAK_ON_ERROR;

		return true;
	}

	bool Texture::AllocateMemory(const TextureDesc& desc, const VkMemoryRequirements& memRequirements)
	{
		m_memoryBlock = m_device->GetDeviceAllocator().Alloc(memRequirements, PB_MEMORY_TYPE_DEVICE_LOCAL);
		if (!m_memoryBlock.m_memory)
		{
			PB_ASSERT(false, "Failed to allocate texture memory block.");
			return false;
		}
		return true;
	}

	void Texture::InitializeMemory(const TextureDesc& desc)
	{
		PB_ASSERT(!((desc.m_initOptions & PB_TEXTURE_INIT_USE_DATA) && (desc.m_initOptions & PB_TEXTURE_INIT_ZERO_INITIALIZE)), "Incompatible texture initialization flags provided: PB_TEXTURE_INIT_USE_DATA, and PB_TEXTURE_INIT_ZERO_INITIALIZE.");

		if (desc.m_initOptions & PB_TEXTURE_INIT_ZERO_INITIALIZE)
		{
			PB_ASSERT(m_availableStates & PB_TEXTURE_STATE_COPY_DST, "Cannot zero initialize an image which does not support copy dst.");

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
			VkClearColorValue clearColor = { 0, 0, 0, 0 };
			VkImageSubresourceRange subresources;
			subresources.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresources.baseMipLevel = 0;
			subresources.baseArrayLayer = 0;
			subresources.layerCount = 1;
			subresources.levelCount = 1;
			vkCmdClearColorImage(internalContext.GetCmdBuffer(), m_image, ConvertPBStateToImageLayout(m_currentState), &clearColor, 1, &subresources); // TODO: Zero initialize support for depth-stencil images.

			internalContext.End();
			internalContext.Return();
		}
		else if (desc.m_initOptions & PB_TEXTURE_INIT_USE_DATA)
		{
			PB_NOT_IMPLEMENTED; // TODO: Create a staging buffer, map it with the user data and copy it to the image memory.
		}
	}
}