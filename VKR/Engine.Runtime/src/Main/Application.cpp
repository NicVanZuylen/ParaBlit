#include "Application.h"
#include "Input.h"
#include "WindowHandle.h"
#include "CLib/Vector.h"

#include <iostream>
#include <chrono>
#include <string>

#include "Engine.ParaBlit/IRenderer.h"

#include "ClientPlayground.h"

namespace Eng
{
	bool Application::m_windowResize = false;

	Application::Application()
	{
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
		for (int i = 0; i < argumentCount; ++i)
		{
			std::string arg = argumentVector[i];
			if (arg[1] == 'w' && arg[2] == '=')
				m_defaultWindowWidth = (uint32_t)std::atoi(&arg.c_str()[3]);
			else if (arg[1] == 'h' && arg[2] == '=')
				m_defaultWindowHeight = (uint32_t)std::atoi(&arg.c_str()[3]);
			else if (arg[1] == 'f' && arg[2] == 'p' && arg[3] == 's' && arg[4] == '=')
				m_fpsCap = (float)std::atoi(&arg.c_str()[5]);
			else if (arg[1] == 'f' && arg[2] == 's' && arg[3] == 0)
				m_isfullScreen = true;
		}

		m_glfwInitialized = false;

		if (!glfwInit())
			return -1;

		m_glfwInitialized = true;

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Required for custom window position.

		// Create window.
		CreateWindowObject(m_defaultWindowWidth, m_defaultWindowHeight, m_isfullScreen);

		printf_s("Window has been created.\n");

		uint32_t extCount = 0;
		auto extNames = glfwGetRequiredInstanceExtensions(&extCount);

		PB::WindowDesc windowInfo = { (HINSTANCE)VKRClient::GetWindowInstance(), (HWND)VKRClient::GetWindowHandle(m_window) };
		PB::RendererDesc rendererDesc;
		rendererDesc.m_extensionCount = extCount;
		rendererDesc.m_extensionNames = extNames;
		rendererDesc.m_windowInfo = &windowInfo;

		m_renderer = PB::CreateRenderer();
		m_renderer->Init(rendererDesc);

		PB::SwapChainDesc swapchainDesc;
		swapchainDesc.m_width = 0;  // Leaving zero will use the full width of the window.
		swapchainDesc.m_height = 0;
		swapchainDesc.m_presentMode = PB::EPresentMode::FIFO;
		swapchainDesc.m_imageCount = 3;

		m_swapchain = m_renderer->CreateSwapChain(swapchainDesc);

		// Initialize input.
		Input::Create();
		m_input = Input::GetInstance();

		printf_s("Input initalized.\n");

		return 0;
	}

	void Application::Run()
	{
		ClientPlayground playground(m_renderer, &m_allocator);

		bool updateDebugMetrics = false;
		float endFrameStallTime = 0.0f;
		while (!glfwWindowShouldClose(m_window))
		{
			// Time
			auto startTime = std::chrono::high_resolution_clock::now();

			// Quit if escape is pressed.
			if (m_input->GetKey(GLFW_KEY_ESCAPE))
				glfwSetWindowShouldClose(m_window, 1);

			// ------------------------------------------------------------------------------------
			// Event Polling
			glfwPollEvents();
			// ------------------------------------------------------------------------------------

			// ------------------------------------------------------------------------------------
			// Render Update
			if (!m_isMinimized)
			{
				if (m_updateRendererResolution)
				{
					int width = 0;
					int height = 0;
					glfwGetWindowSize(m_window, &width, &height);

					m_renderer->WaitIdle();
					playground.UpdateResolution((uint32_t)width, (uint32_t)height);
					m_updateRendererResolution = false;
				}

				playground.Update(m_window, m_input, m_deltaTime, m_elapsedTime, endFrameStallTime, updateDebugMetrics);
				updateDebugMetrics = false;
			}

			// ------------------------------------------------------------------------------------
			// Window Handling

			// Fullscreen Toggle
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
					CreateWindowObject(m_defaultWindowWidth, m_defaultWindowHeight, false);
				}

				m_input->ResetStates();
				m_windowResize = true;
			}

			if (m_windowResize)
			{
				int width = 0;
				int height = 0;
				glfwGetWindowSize(m_window, &width, &height);

				m_isMinimized = (width * height == 0);
				if (!m_isMinimized)
				{
					PB::SwapChainDesc swapchainDesc;
					swapchainDesc.m_width = (uint32_t)width;
					swapchainDesc.m_height = (uint32_t)height;
					swapchainDesc.m_presentMode = PB::EPresentMode::FIFO;
					swapchainDesc.m_imageCount = 3;

					PB::WindowDesc windowInfo = { (HINSTANCE)VKRClient::GetWindowInstance(), (HWND)VKRClient::GetWindowHandle(m_window) };
					m_renderer->RecreateSwapchain(swapchainDesc, &windowInfo);
					m_updateRendererResolution = true; // Signal resolution update for next frame.
					m_windowResize = false;
				}
			}
			// ------------------------------------------------------------------------------------

			// ------------------------------------------------------------------------------------
			// Render End Frame
			if (!m_isMinimized)
				m_renderer->EndFrame(endFrameStallTime);
			// ------------------------------------------------------------------------------------

			m_input->EndFrame();

			// End time...
			std::chrono::steady_clock::time_point endTime;

			// Reset deltatime.
			m_deltaTime = 0.0f;

			// Framerate limitation...
			// Wait for deltatime to reach value based upon frame cap.
			uint64_t timeDuration;
			float fpsCap = m_isMinimized ? std::fminf(10.0f, m_fpsCap) : m_fpsCap;
			while (m_deltaTime < (1000.0f / fpsCap) / 1000.0f)
			{
				endTime = std::chrono::high_resolution_clock::now();
				timeDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
				m_deltaTime = static_cast<float>(static_cast<double>(timeDuration) * 9.99999997e-07f);
			}

			// Get deltatime and add to elapsed time.
			m_elapsedTime += m_deltaTime;
			m_debugDisplayTime -= m_deltaTime;

			// Display frametime and FPS.
			if (m_debugDisplayTime <= 0.0f)
			{
				if (m_displayPerfMetrics)
				{
					double frameTime = double(m_deltaTime * 1000.0f);
					printf_s("Frametime %f ms\n", frameTime);
					printf_s("Elapsed Time: %f\n", m_elapsedTime);
					printf_s("FPS: %i\n", (int)ceilf((1.0f / m_deltaTime)));
				}

				m_debugDisplayTime = m_debugDisplayInterval;
				updateDebugMetrics = true;
			}
		}

		// Wait for idle before shutdown, to ensure no resources are deleted while in-flight.
		m_renderer->WaitIdle();
	}

	void Application::CreateWindowObject(const unsigned int& nWidth, const unsigned int& nHeight, bool bFullScreen)
	{
		if (m_window)
			glfwDestroyWindow(m_window);

		// Create window.
		if (bFullScreen)
		{
			m_window = glfwCreateWindow(nWidth, nHeight, "ParaBlit Render Engine Client", glfwGetPrimaryMonitor(), 0);
		}
		else
		{
			m_window = glfwCreateWindow(nWidth, nHeight, "ParaBlit Render Engine Client", 0, 0);

			// Position window in the centre of the primary monitor.
			const GLFWvidmode* vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
			uint32_t w = (uint32_t)vidMode->width;
			uint32_t h = (uint32_t)vidMode->height;

			glfwSetWindowPos(m_window, (w / 2) - (nWidth / 2), (h / 2) - (nHeight / 2));
			glfwShowWindow(m_window);
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
		Input::GetInstance()->GetCurrentState()[key] = action;
	}

	void Application::MouseButtonCallBack(GLFWwindow* window, int button, int action, int mods)
	{
		Input::GetInstance()->GetCurrentMouseState()->m_buttons[button] = action - 1;
	}

	void Application::CursorPosCallBack(GLFWwindow* window, double dXPos, double dYPos)
	{
		MouseState* currentState = Input::GetInstance()->GetCurrentMouseState();

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
		m_windowResize = true;
	}
};