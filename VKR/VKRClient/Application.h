#pragma once
#include "Time.h"
#include "CLib/Allocator.h"

class Input;

namespace PB
{
	class IRenderer;
	class ISwapChain;
}

struct GLFWwindow;

class Application
{
public:

	Application();

	~Application();

	int Init(int argumentCount, char** argumentVector);

	void Run();

private:

	void CreateWindowObject(const unsigned int& nWidth, const unsigned int& nHeight, bool bFullScreen = false);

	// GLFW Callbacks
	static void ErrorCallBack(int error, const char* desc);
	static void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void MouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallBack(GLFWwindow* window, double dXPos, double dYPos);
	static void MouseScrollCallBack(GLFWwindow* window, double dXOffset, double dYOffset);
	static void WindowResizeCallback(GLFWwindow* window, int nWidth, int nHeight);

	static bool m_windowResize;

	CLib::Allocator m_allocator{};
	PB::IRenderer* m_renderer = nullptr;
	PB::ISwapChain* m_swapchain = nullptr;
	GLFWwindow* m_window = nullptr;
	Input* m_input = nullptr;

	uint32_t m_defaultWindowWidth = 1280;
	uint32_t m_defaultWindowHeight = 720;
	float m_fpsCap = 10000.0f;
	float m_debugDisplayInterval = 0.5f;
	float m_deltaTime = 0.0f;
	float m_elapsedTime = 0.0f;
	float m_debugDisplayTime = 0.0f;

	bool m_isfullScreen = false;
	bool m_glfwInitialized = false;
	bool m_displayPerfMetrics = false;
	bool m_updateRendererResolution = false;
	bool m_isMinimized = false;
};

