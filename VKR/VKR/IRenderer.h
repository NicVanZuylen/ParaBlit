#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"
#include "ISwapChain.h"
#include "IFramebufferCache.h"
#include "IRenderPassCache.h"
#include "IShaderModule.h"
#include "IPipelineCache.h"
#include "ITexture.h"
#include "IBufferObject.h"
#include "IResourcePool.h"

#ifdef PARABLIT_WINDOWS
#pragma warning(push, 0)
#include <Windows.h>
#pragma warning(pop)
#endif

namespace PB 
{
#ifdef PARABLIT_WINDOWS
	struct Win32WindowInfo
	{
		struct HINSTANCE__* m_instance;
		struct HWND__* m_handle;
	};
	using WindowDesc = Win32WindowInfo;
#else
	using WindowDesc = void*;
#endif

	struct RendererDesc
	{
		const char** m_extensionNames = nullptr;
		u32 m_extensionCount = 0;

		// Platform-native window info.
		WindowDesc* m_windowInfo;
	};

	class ICommandList;

	class IRenderer
	{
	public:
		PARABLIT_INTERFACE void Init(const RendererDesc& desc) = 0;

		/*
		Description: Create a swapchain for the renderer with the provided desc. The renderer will automatically present images to the window with it.
		Return Type: ISwapChain*
		Param:
			const SwapChainDesc& desc
		*/
		PARABLIT_INTERFACE ISwapChain* CreateSwapChain(const SwapChainDesc& desc) = 0;
		PARABLIT_INTERFACE void RecreateSwapchain(const SwapChainDesc& desc, WindowDesc* windowDesc) = 0;

		/*
		Description: Submit encoding for this frame and prepare the next frame.
		Param:
			float& outStallTimeMs: Amount of time in ms spent waiting for the corresponding frame-in-flight to complete.
		*/
		PARABLIT_INTERFACE void EndFrame(float& outStallTimeMs) = 0;
		PARABLIT_INTERFACE void WaitIdle() = 0;

		PARABLIT_INTERFACE u32 GetCurrentSwapchainImageIndex() = 0;


		PARABLIT_INTERFACE ITexture* AllocateTexture(const TextureDesc& texDesc) = 0;
		PARABLIT_INTERFACE void FreeTexture(ITexture* texture) = 0;
		PARABLIT_INTERFACE IBufferObject* AllocateBuffer(const BufferObjectDesc& bufDesc) = 0;
		PARABLIT_INTERFACE void FreeBuffer(IBufferObject* buffer) = 0;
		PARABLIT_INTERFACE IResourcePool* AllocateResourcePool(const ResourcePoolDesc& poolDesc) = 0;
		PARABLIT_INTERFACE void FreeResourcePool(IResourcePool* pool) = 0;
		PARABLIT_INTERFACE ResourceView GetSampler(const SamplerDesc& samplerDesc) = 0;
		PARABLIT_INTERFACE void FreeCommandList(ICommandList* list) = 0;

		PARABLIT_INTERFACE ISwapChain* GetSwapchain() = 0;
		PARABLIT_INTERFACE IRenderPassCache* GetRenderPassCache() = 0;;
		PARABLIT_INTERFACE IShaderModuleCache* GetShaderModuleCache() = 0;
		PARABLIT_INTERFACE IPipelineCache* GetPipelineCache() = 0;
		PARABLIT_INTERFACE IFramebufferCache* GetFramebufferCache() = 0;

	private:
		PARABLIT_INTERFACE void CreateWindowSurface(WindowDesc* windowHandle) = 0;
	};

	PARABLIT_API IRenderer* CreateRenderer();
	PARABLIT_API void DestroyRenderer(IRenderer*& renderer);
}