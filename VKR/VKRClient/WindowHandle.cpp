#include "WindowHandle.h"
#include "glfw3native.h"

void* VKRClient::GetWindowHandle(GLFWwindow* window)
{
#ifdef GLFW_EXPOSE_NATIVE_WIN32
	return (void*)glfwGetWin32Window(window);
#endif
}

void* VKRClient::GetWindowInstance()
{
	return (void*)GetModuleHandle(NULL);
}
