#include "Texture.h"
#include "Renderer.h"
#include "Device.h"
#include "CommandContext.h"

namespace PB 
{
	Texture::Texture()
	{

	}

	Texture::~Texture()
	{
		
	}

	void Texture::Create(Renderer* renderer, WrappedTextureDesc desc)
	{
		m_device = renderer->GetDevice();
		m_image = desc.m_wrappedImage;
		m_currentUsage = desc.m_currentUsage;
		m_availableUsage = desc.m_usageFlags;
		m_ownsImage = false;
	}

	void Texture::Destroy()
	{
		if (m_ownsImage)
		{
			vkDestroyImage(m_device->GetHandle(), m_image, nullptr);
		}

		m_image = VK_NULL_HANDLE;
		m_currentUsage = PB_TEXTURE_STATE_NONE;
		m_availableUsage = PB_TEXTURE_STATE_NONE;
	}
}