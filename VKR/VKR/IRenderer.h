#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"
#include "ISwapChain.h"

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

	class IRenderer
	{
	public:
		PARABLIT_INTERFACE void Init(const RendererDesc& desc) = 0;
		PARABLIT_INTERFACE void CreateSwapChain(const SwapChainDesc& desc) = 0;
		PARABLIT_INTERFACE void BeginFrame() = 0;
		PARABLIT_INTERFACE void EndFrame() = 0;

	private:
		PARABLIT_INTERFACE void CreateWindowSurface(WindowDesc* windowHandle) = 0;
	};

	PARABLIT_API IRenderer* CreateRenderer();
	PARABLIT_API void DestroyRenderer(IRenderer*& renderer);
}