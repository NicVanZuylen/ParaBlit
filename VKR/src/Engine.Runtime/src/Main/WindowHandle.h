#pragma once

#pragma warning(push)
#pragma warning (disable : 4005)
#include "GLFW/glfw3.h"
#pragma warning (pop)

namespace Eng
{
	// Get platform-native window information

	void* GetWindowHandle(GLFWwindow* window);
	void* GetWindowInstance();
	void* GetWindowDisplay();
};

