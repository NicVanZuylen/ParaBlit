#include "Application.h"
#include "Input.h"
#include "WindowHandle.h"
#include "CLib/Vector.h"

#include <iostream>
#include <chrono>
#include <string>

#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/IImGUIModule.h"

#include "GameInstanceMain.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"

#if ENG_WINDOWS
#define ENG_USE_MANUAL_WINDOW_POSITION 1
typedef std::chrono::steady_clock::time_point TimePoint;
#else
#define ENG_USE_MANUAL_WINDOW_POSITION 0
typedef std::chrono::system_clock::time_point TimePoint;
#endif

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

		if constexpr (ENG_EDITOR)
		{
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}

		// Destroy window.
		glfwDestroyWindow(m_window);

		// Destroy input.
		Input::Destroy();

		glfwTerminate();

		// Dump any memory leaks.
		m_allocator.DumpMemoryLeaks();
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
			else if (arg[1] == 'v' && arg[2] == 's' && arg[3] == '=')
				m_useVsync = bool(std::atoi(&arg.c_str()[4]) != 0);
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

		printf("[Main] Window has been created.\n");

		CLib::Vector<const char*, 8> requiredVkExtensions;
		{
			uint32_t extCount = 0;
			const char** extNames = glfwGetRequiredInstanceExtensions(&extCount);

			for(uint32_t i = 0; i < extCount; ++i)
			{
				requiredVkExtensions.PushBack(extNames[i]);
			}
		}

#if ENG_WINDOWS
		PB::WindowDesc windowInfo = { (HINSTANCE)GetWindowInstance(), (HWND)GetWindowHandle(m_window) };
#elif ENG_LINUX
		PB::WindowDesc windowInfo = { (Display*)GetWindowDisplay(), (Window)GetWindowHandle(m_window) };

		requiredVkExtensions.PushBack("VK_KHR_xlib_surface");
#endif

		PB::RendererDesc rendererDesc;
		rendererDesc.m_extensionCount = requiredVkExtensions.Count();
		rendererDesc.m_extensionNames = requiredVkExtensions.Data();
		rendererDesc.m_windowInfo = &windowInfo;

		PB::DeviceDesc& deviceDesc = rendererDesc.m_deviceDesc;
		deviceDesc.enableSwapchainExtension = true;
		deviceDesc.enableRaytracingCapabilities = false;

		m_renderer = PB::CreateRenderer();
		m_renderer->Init(rendererDesc);

		PB::SwapChainDesc& swapchainDesc = m_swapchainDesc;
		swapchainDesc.m_width = 0;  // Leaving zero will use the full width of the window.
		swapchainDesc.m_height = 0;
		swapchainDesc.m_presentMode = m_useVsync ? PB::EPresentMode::FIFO : PB::EPresentMode::MAILBOX;
		swapchainDesc.m_imageCount = 3;

		m_swapchain = m_renderer->CreateSwapChain(swapchainDesc);

		// -------- ImGUI initialization --------
		if constexpr (ENG_EDITOR)
		{
			ImGuiContext* imguiContext = ImGui::CreateContext();
			ImGui_ImplGlfw_InitForVulkan(m_window, true);
			PB::IImGUIModule* imguiModule = m_renderer->InitImGUIModule(imguiContext);
		}
		// -------- ImGUI initialization --------

		// Initialize input.
		Input::Create();
		m_input = Input::GetInstance();

		printf("[Main] Input initalized.\n");

		return 0;
	}

	void Application::Run()
	{
		if constexpr (ENG_EDITOR)
		{
			ImGui_ImplGlfw_NewFrame();
		}
		GameInstanceMain* gameMain = new GameInstanceMain(m_input, m_renderer, &m_allocator);

		bool updateDebugMetrics = false;
		while (!glfwWindowShouldClose(m_window))
		{
			// Time
			auto startTime = std::chrono::high_resolution_clock::now();

			// Get deltatime and add to elapsed time.
			m_totalElapsedTime += m_deltaTime;
			m_debugDisplayTime -= m_deltaTime;

			// Quit if escape is pressed.
			if (m_input->GetKey(KEYBOARD_KEY_ESCAPE))
				glfwSetWindowShouldClose(m_window, 1);

			// ------------------------------------------------------------------------------------
			// Event Polling
			glfwPollEvents();
			// ------------------------------------------------------------------------------------

			// ------------------------------------------------------------------------------------
			// Render Update

			if constexpr (ENG_EDITOR)
			{
				ImGuiContext* ctx = ImGui::GetCurrentContext();

				if (ctx->WithinFrameScope == false)
				{
					ImGui_ImplGlfw_NewFrame();
				}
			}

			if (!m_isMinimized)
			{
				if (m_updateRendererResolution)
				{
					int width = 0;
					int height = 0;
					glfwGetWindowSize(m_window, &width, &height);

					m_renderer->WaitIdle();
					gameMain->UpdateResolution((uint32_t)width, (uint32_t)height);
					m_updateRendererResolution = false;
				}

				g_timeMain.m_deltaTimeMain = m_deltaTime;
				g_timeMain.m_totalElapsedTime = m_totalElapsedTime;

				while (g_timeMain.m_simStallTime >= TimeMain::SimUpdateInterval)
				{
					g_timeMain.m_simStallTime -= TimeMain::SimUpdateInterval;
					gameMain->Update(m_window, updateDebugMetrics);
					updateDebugMetrics = false;
				}

				gameMain->Render(m_window, g_timeMain.m_simStallTime / TimeMain::SimUpdateInterval);
			}

			// ------------------------------------------------------------------------------------
			// Window Handling

			// Fullscreen Toggle
			if (ENG_EDITOR == 0 && m_input->GetKey(KEYBOARD_KEY_F11) && !m_input->GetKey(KEYBOARD_KEY_F11, INPUTSTATE_PREVIOUS))
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
					PB::SwapChainDesc& swapchainDesc = m_swapchainDesc;
					swapchainDesc.m_width = (uint32_t)width;
					swapchainDesc.m_height = (uint32_t)height;

#if ENG_WINDOWS
					PB::WindowDesc windowInfo = { (HINSTANCE)GetWindowInstance(), (HWND)GetWindowHandle(m_window) };
					m_renderer->RecreateSwapchain(swapchainDesc, &windowInfo);
#elif ENG_LINUX
					PB::WindowDesc windowInfo = { (Display*)GetWindowDisplay(), (Window)GetWindowHandle(m_window) };
					m_renderer->RecreateSwapchain(swapchainDesc, &windowInfo);
#endif
					m_updateRendererResolution = true; // Signal resolution update for next frame.
					m_windowResize = false;
				}
			}
			// ------------------------------------------------------------------------------------

			m_input->EndFrame();

			// ------------------------------------------------------------------------------------
			// Render End Frame
			if (!m_isMinimized)
				m_renderer->EndFrame(g_timeMain.m_renderStallTime);
			// ------------------------------------------------------------------------------------

			// End time...
			TimePoint endTime;

			// Reset deltatime.
			m_deltaTime = 0.0;

			// Framerate limitation...
			// Wait for deltatime to reach value based upon frame cap.
			double fpsCap = m_isMinimized ? std::fmin(10.0, m_fpsCap) : m_fpsCap;
			while (m_deltaTime < (1000.0 / fpsCap) / 1000.0)
			{
				endTime = std::chrono::high_resolution_clock::now();

				auto timeDuration = std::chrono::duration<double>(endTime - startTime);
				m_deltaTime = timeDuration.count();
			}
			g_timeMain.m_simStallTime += m_deltaTime;

			// Display frametime and FPS.
			if (m_debugDisplayTime <= 0.0)
			{
				if (m_displayPerfMetrics)
				{
					double frameTime = m_deltaTime * 1000.0;
					printf("[Main] Frametime %f ms\n", float(frameTime));
					printf("[Main] Elapsed Time: %f\n", float(m_totalElapsedTime));
					printf("[Main] FPS: %i\n", (int)ceil((1.0 / m_deltaTime)));
				}

				m_debugDisplayTime = m_debugDisplayInterval;
				updateDebugMetrics = true;
			}
		}

		printf("[Main] Window closure requested. Shutting down...\n");

		// Wait for idle before shutdown, to ensure no resources are deleted while in-flight.
		m_renderer->WaitIdle();
		delete gameMain;
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

			if constexpr (ENG_USE_MANUAL_WINDOW_POSITION)
			{
				glfwSetWindowPos(m_window, (w / 2) - (nWidth / 2), (h / 2) - (nHeight / 2));
			}
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
		std::cout << "[Main] GLFW Error: " << desc << "\n";
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