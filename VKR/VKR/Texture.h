#pragma once
#include "ParaBlitApi.h"
#include "vulkan/vulkan.h"

namespace PB 
{
	class Renderer;
	class Device;

	enum TextureState : u16
	{
		PB_TEXTURE_STATE_NONE,
		PB_TEXTURE_STATE_RAW,
		PB_TEXTURE_STATE_RENDERTARGET,
		PB_TEXTURE_STATE_DEPTHTARGET,
		PB_TEXTURE_STATE_SAMPLED,
		PB_TEXTURE_STATE_COPY_SRC,
		PB_TEXTURE_STATE_COPY_DST,
		PB_TEXTURE_STATE_PRESENT
	};

	inline TextureState operator | (TextureState a, TextureState b)
	{
		return static_cast<TextureState>(static_cast<int>(a) | static_cast<int>(b));
	}

	struct WrappedTextureDesc
	{
		VkImage m_wrappedImage = VK_NULL_HANDLE;
		TextureState m_currentUsage = PB_TEXTURE_STATE_NONE;
		TextureState m_usageFlags = PB_TEXTURE_STATE_NONE;
	};

	class Texture
	{
	public:

		PARABLIT_API Texture();

		PARABLIT_API ~Texture();

		PARABLIT_API void Create(Renderer* renderer, WrappedTextureDesc desc);

		PARABLIT_API void Destroy();

	private:

		Device* m_device = nullptr;
		VkImage m_image = VK_NULL_HANDLE;
		TextureState m_availableUsage = PB_TEXTURE_STATE_NONE;
		TextureState m_currentUsage = PB_TEXTURE_STATE_NONE;
		bool m_ownsImage = true; // There may be some cases (Such as swap chain images), where the VkImage is already created for us. This flags (if false) for this object to not delete the VkImage when destroyed.
	};
}

