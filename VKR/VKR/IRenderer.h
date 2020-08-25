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

#ifdef PARABLIT_WINDOWS
#include <Windows.h>
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

	enum ESamplerFilter
	{
		PB_SAMPLER_FILTER_NEAREST,
		PB_SAMPLER_FILTER_BILINEAR,
	};

	enum ESamplerRepeatMode
	{
		PB_SAMPLER_REPEAT_REPEAT,
		PB_SAMPLER_REPEAT_MIRRORED_REPEAT,
		PB_SAMPLER_REPEAT_CLAMP,
	};

	struct SamplerDesc
	{
		ESamplerFilter m_filter = PB_SAMPLER_FILTER_BILINEAR;
		ESamplerFilter m_mipFilter = PB_SAMPLER_FILTER_BILINEAR;
		ESamplerRepeatMode m_repeatMode = PB_SAMPLER_REPEAT_REPEAT;
		float m_anisotropyLevels = 0.0f;

		bool operator == (const SamplerDesc& other) const;
	};

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
		PARABLIT_INTERFACE void EndFrame() = 0;
		PARABLIT_INTERFACE void WaitIdle() = 0;

		PARABLIT_INTERFACE u32 GetCurrentSwapchainImageIndex() = 0;

		PARABLIT_INTERFACE ITexture* AllocateTexture(const TextureDesc& texDesc) = 0;
		PARABLIT_INTERFACE void FreeTexture(ITexture* texture) = 0;
		PARABLIT_INTERFACE IBufferObject* AllocateBuffer(const BufferObjectDesc& bufDesc) = 0;
		PARABLIT_INTERFACE void FreeBuffer(IBufferObject* buffer) = 0;
		PARABLIT_INTERFACE Sampler GetSampler(const SamplerDesc& samplerDesc) = 0;

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