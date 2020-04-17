#include "Application.h"
#include "Input.h"
#include "ParaBlitLog.h"
#include "WindowHandle.h"

#include <iostream>
#include <chrono>

#include "glfw3.h"
#include "IRenderer.h"

GLFWwindow* Application::m_window = nullptr;
Input* Application::m_input = nullptr;
bool Application::m_isfullScreen = false;
bool Application::m_glfwInitialized = false;

Application::Application()
{
	deltaTime = 0.0f;
	elapsedTime = 0.0f;
	debugDisplayTime = DEBUG_DISPLAY_TIME;
}

Application::~Application()
{
	if (m_glfwInitialized)
		glfwTerminate();
	else
		return;

	// Destroy window.
	glfwDestroyWindow(m_window);

	// Destroy input.
	Input::Destroy();
}

int Application::Init() 
{
	m_glfwInitialized = false;

	if (!glfwInit())
		return -1;

	m_glfwInitialized = true;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	// Create window.
	CreateWindowObject(WINDOW_WIDTH, WINDOW_HEIGHT, m_isfullScreen);

	PB_LOG("Window has been created.");
	
	uint32_t extCount = 0;
	auto extNames = glfwGetRequiredInstanceExtensions(&extCount);

	PB::WindowDesc windowInfo = { (HINSTANCE)VKRClient::GetWindowInstance(), (HWND)VKRClient::GetWindowHandle(m_window) };
	PB::RendererDesc rendererDesc = { extNames, extCount, &windowInfo };
	PB::IRenderer* renderer = PB::CreateRenderer();
	renderer->Init(rendererDesc);
	
	PB::SwapChainDesc swapchainDesc;
	swapchainDesc.m_width = 0;
	swapchainDesc.m_height = 0;
	swapchainDesc.m_presentMode = PB::VKR_PRESENT_MODE_MAILBOX;
	swapchainDesc.m_imageCount = 3;

	renderer->CreateSwapChain(swapchainDesc);

	PB::DestroyRenderer(renderer);

	// Initialize input.
	Input::Create();
	m_input = Input::GetInstance();

	PB_LOG("Input initalized.");

	return 0;
}

void Application::Run() 
{
	while(!glfwWindowShouldClose(m_window)) 
	{
		// Time
		auto startTime = std::chrono::high_resolution_clock::now();

		// Quit if escape is pressed.
		if (m_input->GetKey(GLFW_KEY_ESCAPE))
			glfwSetWindowShouldClose(m_window, 1);

		// ------------------------------------------------------------------------------------

		// Poll events.
		glfwPollEvents();

		// Fullscreen
		if (m_input->GetKey(GLFW_KEY_F11) && !m_input->GetKey(GLFW_KEY_F11, INPUTSTATE_PREVIOUS))
		{
			// Get primary monitor and video mode.
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* vidMode = glfwGetVideoMode(monitor);

			m_isfullScreen = !m_isfullScreen;

			// Recreate window.
			if (m_isfullScreen) 
			{
			    CreateWindowObject(vidMode->width, vidMode->height, true);
			}
			else 
			{
				CreateWindowObject(WINDOW_WIDTH, WINDOW_HEIGHT, false);
			}

			m_input->ResetStates();
		}

		m_input->EndFrame();

		// End time...
		std::chrono::steady_clock::time_point endTime;
		long long timeDuration;

		// Reset deltatime.
		deltaTime = 0.0f;

		// Framerate limitation...
		// Wait for deltatime to reach value based upon frame cap.
		while(deltaTime < (1000.0f / FRAMERATE_CAP) / 1000.0f)
		{
			endTime = std::chrono::high_resolution_clock::now();
			timeDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

			deltaTime = static_cast<float>(timeDuration) / 1000000.0f;
		}

		// Get deltatime and add to elapsed time.
		elapsedTime += deltaTime;
		debugDisplayTime -= deltaTime;

#ifdef DISPLAY_FRAME_TIME
		// Display frametime and FPS.
		if(debugDisplayTime <= 0.0f) 
		{
			float frameTime = deltaTime * 1000.0f;
			PB_LOG_FORMAT("Frametime %f ms", frameTime);
			PB_LOG_FORMAT("Elapsed Time: %f", elapsedTime);
			PB_LOG_FORMAT("FPS: %i", (int)ceilf((1.0f / deltaTime)));

			debugDisplayTime = DEBUG_DISPLAY_TIME;
		}
#endif
	}
}

void Application::CreateWindowObject(const unsigned int& nWidth, const unsigned int& nHeight, bool bFullScreen)
{
	if (m_window)
		glfwDestroyWindow(m_window);

	// Create window.
	if(bFullScreen) 
	{
		m_window = glfwCreateWindow(nWidth, nHeight, "Vulkan Test", glfwGetPrimaryMonitor(), 0);
	}
	else 
	{
		m_window = glfwCreateWindow(nWidth, nHeight, "Vulkan Test", 0, 0);
	}

	// Set key callback...
	glfwSetKeyCallback(m_window, &KeyCallBack);

	// Set mouse callbacks...
	glfwSetMouseButtonCallback(m_window, &MouseButtonCallBack);
	glfwSetCursorPosCallback(m_window, &CursorPosCallBack);
	glfwSetScrollCallback(m_window, &MouseScrollCallBack);

	// Set window resize callback.
	glfwSetFramebufferSizeCallback(m_window, &WindowResizeCallback);
}

void Application::ErrorCallBack(int error, const char* desc)
{
	std::cout << "GLFW Error: " << desc << "\n";
}

void Application::KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	m_input->GetCurrentState()[key] = action;
}

void Application::MouseButtonCallBack(GLFWwindow* window, int button, int action, int mods)
{
	m_input->GetCurrentMouseState()->m_buttons[button] = action - 1;
}

void Application::CursorPosCallBack(GLFWwindow* window, double dXPos, double dYPos)
{
	MouseState* currentState = m_input->GetCurrentMouseState();

	currentState->m_fMouseAxes[0] = dXPos;
	currentState->m_fMouseAxes[1] = dYPos;
}

void Application::MouseScrollCallBack(GLFWwindow* window, double dXOffset, double dYOffset)
{
	MouseState* currentState = Input::GetInstance()->GetCurrentMouseState();

	currentState->m_fMouseAxes[2] = dXOffset;
	currentState->m_fMouseAxes[3] = dYOffset;
}

void Application::WindowResizeCallback(GLFWwindow* window, int nWidth, int nHeight) 
{
	PB_NOT_IMPLEMENTED;
}
