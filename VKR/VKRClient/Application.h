#pragma once
#include "Time.h"

class Input;

namespace PB
{
	class IRenderer;
}

struct GLFWwindow;

#define DEBUG_DISPLAY_TIME 2.0f

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define FRAMERATE_CAP 10000.0f

#define DISPLAY_FRAME_TIME

class Application
{
public:

	Application();

	~Application();

	int Init();

	void Run();

private:

	static void CreateWindowObject(const unsigned int& nWidth, const unsigned int& nHeight, bool bFullScreen = false);

	// GLFW Callbacks
	static void ErrorCallBack(int error, const char* desc);
	static void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void MouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallBack(GLFWwindow* window, double dXPos, double dYPos);
	static void MouseScrollCallBack(GLFWwindow* window, double dXOffset, double dYOffset);
	static void WindowResizeCallback(GLFWwindow* window, int nWidth, int nHeight);

	static PB::IRenderer* m_renderer;
	static GLFWwindow* m_window;
	static Input* m_input;
	static bool m_isfullScreen;
	static bool m_glfwInitialized;

	float deltaTime;
	float elapsedTime;
	float debugDisplayTime;
};

