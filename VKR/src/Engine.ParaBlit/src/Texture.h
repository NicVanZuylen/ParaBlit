#pragma once
#include "ITexture.h"
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"
#include "DeviceAllocator.h"
#include "PoolAllocator.h"

namespace PB 
{
	class IRenderer;
	class Renderer;
	class Device;

	struct WrappedTextureDesc
	{
		VkImage m_wrappedImage = VK_NULL_HANDLE;
		TextureStateFlags m_usageFlags = ETextureState::NONE;
		ETextureFormat m_format = PB::ETextureFormat::B8G8R8A8_UNORM;
		u32 m_width = 0;
		u32 m_height = 0;
	};

	class Texture : ITexture
	{
	public:

		Texture();

		~Texture();

		void Create(IRenderer* renderer, const TextureDesc& desc) override;

		void Create(Renderer* renderer, WrappedTextureDesc desc);

		void Destroy();

		VkImage GetImage();

		TextureStateFlags GetUsage();

		bool CanAlias(ITexture* baseTexture) override;

		void AliasTexture(ITexture* baseTexture) override;

		void GetMemorySizeAndAlign(u32& outSizeBytes, u32& outAlignBytes) override;

		u8* MapReadback() override;

		void UnmapReadback() override;

		ResourceView GetDefaultSRV() override;

		RenderTargetView GetDefaultRTV() override;

		ResourceView GetView(TextureViewDesc& viewDesc) override;

		ResourceView GetDefaultSIV() override;

		ResourceView GetViewAsStorageImage(TextureViewDesc& viewDesc) override;

		RenderTargetView GetRenderTargetView(TextureViewDesc& viewDesc) override;

		VkExtent3D GetExtent();

		void RegisterView(const TextureViewDesc& desc);

		inline u32 GetMipCount() { return m_mipCount; }

		inline u32 GetArrayLayerCount() { return m_arrayLayerCount; }

		inline bool HasDepthPlane() { return m_hasDepthPlane; }

		inline bool HasStencilPlane() { return m_hasStencilPlane; }

	private:

		inline bool CreateImageResource(const TextureDesc& desc);

		inline bool AllocateMemory(const TextureDesc& desc, const VkMemoryRequirements& memRequirements);

		inline void InitializeMemory(const TextureDesc& desc);

		Renderer* m_renderer = nullptr;
		Device* m_device = nullptr;
		VkImage m_image = VK_NULL_HANDLE;
		VkExtent3D m_extents = { 1, 1, 1 };
		PoolAllocator::PoolAllocation m_poolAllocation{};
		EMemoryType m_memoryType = EMemoryType::END_RANGE;
		TextureStateFlags m_availableStates = ETextureState::NONE;
		ETextureFormat m_format = ETextureFormat::UNKNOWN;
		u16 m_mipCount = 1;
		u16 m_arrayLayerCount = 1;
		bool m_ownsImage : 1; // There may be some cases (Such as swap chain images), where the VkImage is already created for us. This flags (if false) for this object to not delete the VkImage when destroyed.
		bool m_hasDepthPlane : 1;
		bool m_hasStencilPlane : 1;
		bool m_isAlias : 1;
		bool m_isMapped : 1;
		CLib::Vector<TextureViewDesc, 1, 8> m_viewDescs;
	};
}

