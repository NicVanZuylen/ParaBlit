#pragma once
#include "ITexture.h"
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"
#include "DeviceAllocator.h"

namespace PB 
{
	class IRenderer;
	class Renderer;
	class Device;

	struct WrappedTextureDesc
	{
		VkImage m_wrappedImage = VK_NULL_HANDLE;
		ETextureState m_currentUsage = ETextureState::NONE;
		TextureStateFlags m_usageFlags = ETextureState::NONE;
		u32 m_width = 0;
		u32 m_height = 0;
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

		PARABLIT_API TextureStateFlags GetUsage();

		PARABLIT_API ETextureState GetState();

		bool CanAlias(ITexture* baseTexture) override;

		void AliasTexture(ITexture* baseTexture) override;

		TextureView GetDefaultSRV() override;

		TextureView GetDefaultRTV() override;

		TextureView GetView(TextureViewDesc& viewDesc) override;

		TextureView GetRenderTargetView(TextureViewDesc& viewDesc) override;

		VkExtent3D GetExtent();

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
		VkExtent3D m_extents = { 1, 1, 1 };
		DeviceAllocator::PageView m_memoryBlock;
		TextureStateFlags m_availableStates = ETextureState::NONE;
		ETextureState m_currentState = ETextureState::NONE;
		ETextureFormat m_format = ETextureFormat::UNKNOWN;
		bool m_ownsImage : 1; // There may be some cases (Such as swap chain images), where the VkImage is already created for us. This flags (if false) for this object to not delete the VkImage when destroyed.
		bool m_hasDepthPlane : 1;
		bool m_hasStencilPlane : 1;
		bool m_isAlias : 1;
		CLib::Vector<TextureViewDesc, 1, 8> m_viewDescs;
	};
}

