#pragma once
#include "ITexture.h"
#include "ParaBlitApi.h"
#include "vulkan/vulkan.h"

namespace PB 
{
	class Renderer;
	class Device;

	struct WrappedTextureDesc
	{
		VkImage m_wrappedImage = VK_NULL_HANDLE;
		ETextureState m_currentUsage = PB_TEXTURE_STATE_NONE;
		ETextureState m_usageFlags = PB_TEXTURE_STATE_NONE;
	};

	class Texture : ITexture
	{
	public:

		PARABLIT_API Texture();

		PARABLIT_API ~Texture();

		PARABLIT_API void Create(Renderer* renderer, WrappedTextureDesc desc);

		PARABLIT_API void Destroy();

		PARABLIT_API VkImage GetImage();

		PARABLIT_API void SetState(ETextureState state);

		PARABLIT_API ETextureState GetUsage();

		PARABLIT_API ETextureState GetState();

	private:

		Device* m_device = nullptr;
		VkImage m_image = VK_NULL_HANDLE;
		ETextureState m_availableStates = PB_TEXTURE_STATE_NONE;
		ETextureState m_currentState = PB_TEXTURE_STATE_NONE;
		bool m_ownsImage = true; // There may be some cases (Such as swap chain images), where the VkImage is already created for us. This flags (if false) for this object to not delete the VkImage when destroyed.
	};
}

