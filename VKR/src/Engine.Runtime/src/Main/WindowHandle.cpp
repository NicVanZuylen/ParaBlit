#include "WindowHandle.h"
#include "GLFW/glfw3native.h"

namespace Eng
{
void* GetWindowHandle(GLFWwindow* window)
{
#ifdef GLFW_EXPOSE_NATIVE_WIN32
	return (void*)glfwGetWin32Window(window);
#elif GLFW_EXPOSE_NATIVE_X11
	return (void*)glfwGetX11Window(window);
#endif
}

void* GetWindowInstance()
{
#if ENG_WINDOWS
	return (void*)GetModuleHandle(NULL);
#else
	return nullptr;
#endif
}

void* GetWindowDisplay()
{
#if GLFW_EXPOSE_NATIVE_X11
	return (void*)glfwGetX11Display();
#else
	return nullptr;
#endif
}
}
