#include "Application.h"
#include "Input.h"
#include "ParaBlitLog.h"
#include "WindowHandle.h"
#include "ICommandContext.h"
#include "CLib/Vector.h"
#include "QuickIO.h"
#include "Camera.h"
#include "Mesh.h"
#include "Texture.h"
#include "Shader.h"

#include "RenderGraph.h"
#include "BasicColorPass.h"

#include <iostream>
#include <chrono>
#include <string>

#include "IRenderer.h"

#pragma warning(push, 0)
#include "glm/include/glm.hpp"
#include "gtc/matrix_transform.hpp"
#pragma warning(pop)

using namespace PBClient;

CLib::Allocator Application::m_allocator;
PB::IRenderer* Application::m_renderer = nullptr;
PB::ISwapChain* Application::m_swapchain = nullptr;
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

RenderGraph* CreateRenderGraph(CLib::Allocator* allocator, PB::IRenderer* renderer, RenderGraphBehaviour** behaviours)
{
	PB::ISwapChain* swapchain = renderer->GetSwapchain();
	RenderGraph* output = nullptr;
	{
		RenderGraphBuilder rgBuilder(renderer, allocator);

		NodeDesc nodeDesc;
		nodeDesc.m_behaviour = behaviours[0];

		AttachmentDesc& colorDesc = nodeDesc.m_attachments[0];
		colorDesc.m_format = swapchain->GetImageFormat();
		colorDesc.m_width = swapchain->GetWidth();
		colorDesc.m_height = swapchain->GetHeight();
		colorDesc.m_name = "ColorOutput";
		colorDesc.m_usage = PB::EAttachmentUsage::COLOR;
		colorDesc.m_flags = EAttachmentFlags::COPY_SRC;
		colorDesc.m_clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		AttachmentDesc& depthDesc = nodeDesc.m_attachments[1];
		depthDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
		depthDesc.m_width = swapchain->GetWidth();
		depthDesc.m_height = swapchain->GetHeight();
		depthDesc.m_name = nullptr; // Unnamed means temporary.
		depthDesc.m_usage = PB::EAttachmentUsage::DEPTHSTENCIL;
		depthDesc.m_clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		nodeDesc.m_attachmentCount = 2;
		nodeDesc.m_renderWidth = colorDesc.m_width;
		nodeDesc.m_renderHeight = colorDesc.m_height;

		rgBuilder.AddNode(nodeDesc);
		output = rgBuilder.Build();
	}
	return output;
}

void Application::Run() 
{
	// Create a rendergraph pass to render our scene with.
	BasicColorPass* node = m_allocator.Alloc<BasicColorPass>(m_renderer, &m_allocator);
	RenderGraph* renderGraph = CreateRenderGraph(&m_allocator, m_renderer, (RenderGraphBehaviour**)&node);

	// Create a buffer for the model, view and projection matrices.
	PB::BufferObjectDesc bufferDesc;
	bufferDesc.m_bufferSize = sizeof(glm::mat4) * 3;
	bufferDesc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
	bufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::UNIFORM;
	PB::IBufferObject* mvpBuffer = m_renderer->AllocateBuffer(bufferDesc);
	node->SetMVPBuffer(mvpBuffer);

	Camera cam(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), 0.5f, 5.0f);

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

		cam.Update(deltaTime, m_input, m_window);

		// Update Camera -------------------------------------------------------------------------------------------------
		{
			glm::mat4* bufferMatrices = (glm::mat4*)mvpBuffer->BeginPopulate();

			glm::mat4 model = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -4.0f));
			model = glm::scale(model, glm::vec3(0.01f));
			model = glm::rotate<float>(model, elapsedTime, glm::vec3(0.0f, 1.0f, 0.0f));

			bufferMatrices[0] = model; // Model
			bufferMatrices[1] = cam.GetViewMatrix(); // View

			glm::mat4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
			axisCorrection[1][1] = -1.0f;
			axisCorrection[2][2] = 1.0f;
			axisCorrection[3][3] = 1.0f;

			bufferMatrices[2] = axisCorrection * glm::perspectiveFov<float>(45.0f, (float)m_swapchain->GetWidth(), (float)m_swapchain->GetHeight(), 0.1f, 1000.0f); // Projection
			mvpBuffer->EndPopulate();
		}
		// ---------------------------------------------------------------------------------------------------------------

		// Change render node output -------------------------------------------------------------------------------------
		{
			PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
			auto* swapChainTex = m_swapchain->GetImage(swapChainIdx);
			node->SetOutputTexture(swapChainTex);
		}
		// ---------------------------------------------------------------------------------------------------------------

		renderGraph->Execute();

		m_renderer->EndFrame();
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

#if DISPLAY_FRAME_TIME
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

	m_renderer->WaitIdle();

	m_renderer->FreeBuffer(mvpBuffer);

	m_allocator.Free(renderGraph);
	m_allocator.Free(node);
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
