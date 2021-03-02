#include "Application.h"
#include "Input.h"
#include "ParaBlitLog.h"
#include "WindowHandle.h"
#include "CLib/Vector.h"
#include "QuickIO.h"

#include <iostream>
#include <chrono>
#include <string>

#include "IRenderer.h"

#include "ClientPlayground.h"

#pragma warning(push, 0)
#include "glm/include/glm.hpp"
#include "gtc/matrix_transform.hpp"
#pragma warning(pop)

CLib::Allocator Application::m_allocator;
PB::IRenderer* Application::m_renderer = nullptr;
PB::ISwapChain* Application::m_swapchain = nullptr;
GLFWwindow* Application::m_window = nullptr;
Input* Application::m_input = nullptr;
bool Application::m_isfullScreen = false;
bool Application::m_glfwInitialized = false;

Application::Application()
{
	m_deltaTime = 0.0f;
	m_elapsedTime = 0.0f;
	m_debugDisplayTime = DEBUG_DISPLAY_TIME;
}

Application::~Application()
{
	if (!m_glfwInitialized)
		return;

	PB::DestroyRenderer(m_renderer);

	// Destroy window.
	glfwDestroyWindow(m_window);

	// Destroy input.
	Input::Destroy();

	glfwTerminate();
}

int Application::Init(int argumentCount, char** argumentVector)
{
	uint32_t windowWidth = WINDOW_WIDTH;
	uint32_t windowHeight = WINDOW_HEIGHT;

	for (int i = 0; i < argumentCount; ++i)
	{
		std::string arg = argumentVector[i];
		if (arg[1] == 'w' && arg[2] == '=')
			windowWidth = (uint32_t)std::atoi(&arg.c_str()[3]);
		else if (arg[1] == 'h' && arg[2] == '=')
			windowHeight = (uint32_t)std::atoi(&arg.c_str()[3]);
		else if (arg[1] == 'f' && arg[2] == 's' && arg[3] == 0)
			m_isfullScreen = true;
	}

	m_glfwInitialized = false;

	if (!glfwInit())
		return -1;

	m_glfwInitialized = true;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	
	// Create window.
	CreateWindowObject(windowWidth, windowHeight, m_isfullScreen);

	PB_LOG("Window has been created.");
	
	uint32_t extCount = 0;
	auto extNames = glfwGetRequiredInstanceExtensions(&extCount);

	PB::WindowDesc windowInfo = { (HINSTANCE)VKRClient::GetWindowInstance(), (HWND)VKRClient::GetWindowHandle(m_window) };
	PB::RendererDesc rendererDesc = { extNames, extCount, &windowInfo };
	m_renderer = PB::CreateRenderer();
	m_renderer->Init(rendererDesc);
	
	PB::SwapChainDesc swapchainDesc;
	swapchainDesc.m_width = 0;  // Leaving zero will use the full width of the window.
	swapchainDesc.m_height = 0;
	swapchainDesc.m_presentMode = PB::EPresentMode::MAILBOX;
	swapchainDesc.m_imageCount = 3;

	m_swapchain = m_renderer->CreateSwapChain(swapchainDesc);

	// Initialize input.
	Input::Create();
	m_input = Input::GetInstance();

	PB_LOG("Input initalized.");

	return 0;
}

void Application::Run() 
{
	ClientPlayground playground(m_renderer, &m_allocator);

	while (!glfwWindowShouldClose(m_window))
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

		playground.Update(m_window, m_input, m_deltaTime, m_elapsedTime);

		m_renderer->EndFrame();
		m_input->EndFrame();

		// End time...
		std::chrono::steady_clock::time_point endTime;
		long long timeDuration;

		// Reset deltatime.
		m_deltaTime = 0.0f;

		// Framerate limitation...
		// Wait for deltatime to reach value based upon frame cap.
		while(m_deltaTime < (1000.0f / FRAMERATE_CAP) / 1000.0f)
		{
			endTime = std::chrono::high_resolution_clock::now();
			timeDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

			m_deltaTime = static_cast<float>(timeDuration) / 1000000.0f;
		}

		// Get deltatime and add to elapsed time.
		m_elapsedTime += m_deltaTime;
		m_debugDisplayTime -= m_deltaTime;

#if DISPLAY_FRAME_TIME
		// Display frametime and FPS.
		if(m_debugDisplayTime <= 0.0f) 
		{
			float frameTime = m_deltaTime * 1000.0f;
			PB_LOG_FORMAT("Frametime %f ms", frameTime);
			PB_LOG_FORMAT("Elapsed Time: %f", m_elapsedTime);
			PB_LOG_FORMAT("FPS: %i", (int)ceilf((1.0f / m_deltaTime)));

			m_debugDisplayTime = DEBUG_DISPLAY_TIME;
		}
#endif
	}

	// Wait for idle before shutdown, to ensure no resources are deleted while in-flight.
	m_renderer->WaitIdle();
}

void Application::CreateWindowObject(const unsigned int& nWidth, const unsigned int& nHeight, bool bFullScreen)
{
	if (m_window)
		glfwDestroyWindow(m_window);

	// Create window.
	if(bFullScreen) 
	{
		m_window = glfwCreateWindow(nWidth, nHeight, "ParaBlit Render Engine Client", glfwGetPrimaryMonitor(), 0);
	}
	else 
	{
		m_window = glfwCreateWindow(nWidth, nHeight, "ParaBlit Render Engine Client", 0, 0);
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
