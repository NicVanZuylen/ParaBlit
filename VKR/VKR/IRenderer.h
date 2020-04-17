#pragma once
#include "VKRApi.h"
#include "VKRInterface.h"
#include "ISwapChain.h"

#ifdef VKR_WINDOWS
#include <Windows.h>
#endif

namespace VKR 
{
#ifdef VKR_WINDOWS
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
		VKR_INTERFACE void Init(const RendererDesc& desc) = 0;
		VKR_INTERFACE void CreateSwapChain(const SwapChainDesc& desc) = 0;

	private:
		VKR_INTERFACE void CreateWindowSurface(WindowDesc* windowHandle) = 0;
	};

	VKR_API IRenderer* CreateRenderer();
	VKR_API void DestroyRenderer(IRenderer*& renderer);
}