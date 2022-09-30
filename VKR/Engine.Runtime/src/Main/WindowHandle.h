#pragma once

#include "GLFW/glfw3.h"

namespace VKRClient
{
	// Get platform-native window information

	void* GetWindowHandle(GLFWwindow* window);
	void* GetWindowInstance();
};

