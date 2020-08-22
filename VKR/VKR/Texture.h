#pragma once
#include "ITexture.h"
#include "ParaBlitApi.h"
#include "vulkan/vulkan.h"
#include "DeviceAllocator.h"

namespace PB 
{
	class IRenderer;
	class Renderer;
	class Device;

	struct WrappedTextureDesc
	{
		VkImage m_wrappedImage = VK_NULL_HANDLE;
		ETextureState m_currentUsage = PB_TEXTURE_STATE_NONE;
		ETextureStateFlags m_usageFlags = PB_TEXTURE_STATE_NONE;
	};

	class Texture : ITexture
	{
	public:

		PARABLIT_API Texture();

		PARABLIT_API ~Texture();

		PARABLIT_API void Create(IRenderer* renderer, const TextureDesc& desc) override;

		PARABLIT_API void Create(Renderer* renderer, WrappedTextureDesc desc);

		PARABLIT_API inline void Destroy();

		PARABLIT_API VkImage GetImage();

		PARABLIT_API void SetState(ETextureState state);

		PARABLIT_API ETextureStateFlags GetUsage();

		PARABLIT_API ETextureState GetState();

		TextureView GetView(TextureViewDesc& viewDesc) override;

		TextureView GetRenderTargetView(TextureViewDesc& viewDesc) override;

		void RegisterView(const TextureViewDesc& desc);

		bool HasDepthPlane();

		bool HasStencilPlane();

	private:

		PARABLIT_API inline bool CreateImageResource(const TextureDesc& desc);

		PARABLIT_API inline bool AllocateMemory(const TextureDesc& desc, const VkMemoryRequirements& memRequirements);

		PARABLIT_API inline void InitializeMemory(const TextureDesc& desc);

		Renderer* m_renderer = nullptr;
		Device* m_device = nullptr;
		VkImage m_image = VK_NULL_HANDLE;
		DeviceAllocator::PageView m_memoryBlock;
		ETextureStateFlags m_availableStates = PB_TEXTURE_STATE_NONE;
		ETextureState m_currentState = PB_TEXTURE_STATE_NONE;
		ETextureFormat m_format = PB_TEXTURE_FORMAT_UNKNOWN;
		bool m_ownsImage : 1; // There may be some cases (Such as swap chain images), where the VkImage is already created for us. This flags (if false) for this object to not delete the VkImage when destroyed.
		bool m_hasDepthPlane : 1;
		bool m_hasStencilPlane : 1;
		CLib::Vector<TextureViewDesc, 1, 8> m_viewDescs;
	};
}

