#include "Texture.h"
#include "ParaBlitDebug.h"
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
		m_currentState = desc.m_currentUsage;
		m_availableStates = desc.m_usageFlags;
		m_ownsImage = false;

		PB_ASSERT(((m_availableStates & m_currentState) || m_currentState == PB_TEXTURE_STATE_NONE));
	}

	void Texture::Destroy()
	{
		if (m_ownsImage)
		{
			vkDestroyImage(m_device->GetHandle(), m_image, nullptr);
		}

		m_image = VK_NULL_HANDLE;
		m_currentState = PB_TEXTURE_STATE_NONE;
		m_availableStates = PB_TEXTURE_STATE_NONE;
	}

	VkImage Texture::GetImage()
	{
		return m_image;
	}

	void Texture::SetState(ETextureState state)
	{
		m_currentState = state;
	}

	ETextureState Texture::GetUsage()
	{
		return m_availableStates;
	}

	ETextureState Texture::GetState()
	{
		return m_currentState;
	}
}