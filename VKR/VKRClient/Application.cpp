#include "Application.h"
#include "Input.h"
#include "ParaBlitLog.h"
#include "WindowHandle.h"
#include "ICommandContext.h"
#include "CLib/Vector.h"
#include "QuickIO.h"

#include <iostream>
#include <chrono>
#include <string>

#include "glfw3.h"
#include "IRenderer.h"
#include "ITexture.h"

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
	swapchainDesc.m_presentMode = PB::VKR_PRESENT_MODE_MAILBOX;
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
	PB::TextureDesc testTexDesc;
	testTexDesc.m_data.m_data = nullptr;
	testTexDesc.m_data.m_size = 0;
	testTexDesc.m_data.m_format = PB::PB_TEXTURE_FORMAT_R8G8B8A8_UNORM;
	testTexDesc.m_initialState = PB::PB_TEXTURE_STATE_COPY_DST;
	testTexDesc.m_usageStates = PB::PB_TEXTURE_STATE_COLORTARGET;
	testTexDesc.m_initOptions = PB::PB_TEXTURE_INIT_ZERO_INITIALIZE;
	testTexDesc.m_width = m_swapchain->GetWidth();
	testTexDesc.m_height = m_swapchain->GetHeight();
	PB::ITexture* testTex = m_renderer->AllocateTexture(testTexDesc);

	PB::TextureViewDesc viewDesc = {};
	viewDesc.m_texture = testTex;
	viewDesc.m_renderer = m_renderer;
	viewDesc.m_format = PB::PB_TEXTURE_FORMAT_R8G8B8A8_UNORM;

	auto testView = m_renderer->GetTextureViewCache()->GetView(viewDesc);

	PB::ShaderModule vertModule(0);
	PB::ShaderModule fragModule(0);

	{
		const char* vsPath = "TestAssets/Shaders/SPIR-V/vs_triangle.spv";
		char* triVertSpv = nullptr;
		size_t triVertSpvSize = 0;
		QIO::Load(vsPath, &triVertSpv, triVertSpvSize);

		PB::ShaderModuleDesc moduleDesc;
		moduleDesc.m_byteCode = triVertSpv;
		moduleDesc.m_size = triVertSpvSize;
		moduleDesc.m_key = vsPath;
		moduleDesc.m_keySize = strlen(vsPath);
		vertModule = m_renderer->GetShaderModuleCache()->GetModule(moduleDesc);
		delete[] triVertSpv;

		const char* fsPath = "TestAssets/Shaders/SPIR-V/fs_triangle.spv";
		char* triFragSpv = nullptr;
		size_t triFragSpvSize = 0;
		QIO::Load(fsPath, &triFragSpv, triFragSpvSize);

		moduleDesc.m_byteCode = triFragSpv;
		moduleDesc.m_size = triFragSpvSize;
		moduleDesc.m_key = fsPath;
		moduleDesc.m_keySize = strlen(fsPath);
		fragModule = m_renderer->GetShaderModuleCache()->GetModule(moduleDesc);
		delete[] triFragSpv;
	}

	CLib::Vector<PB::TextureView, 3> swapchainTextureViews;
	for (unsigned int i = 0; i < m_swapchain->GetImageCount(); ++i)
	{
		PB::TextureViewDesc desc = {};
		desc.m_texture = m_swapchain->GetImage(i);
		desc.m_renderer = m_renderer;
		desc.m_format = PB::PB_TEXTURE_FORMAT_B8G8R8A8_UNORM;
		swapchainTextureViews.PushBack(m_renderer->GetTextureViewCache()->GetView(desc));
	}

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

		m_renderer->BeginFrame();

		PB::RenderPassDesc rpDesc;
		PB::AttachmentDesc attachments[] =
		{
			{ PB::PB_TEXTURE_FORMAT_B8G8R8A8_UNORM, PB::PB_TEXTURE_STATE_COLORTARGET, PB::PB_TEXTURE_STATE_COLORTARGET, PB::PB_ATTACHMENT_START_ACTION_CLEAR },
		};
		PB::SubpassDesc subpassDescs[] =
		{
			{ {PB::PB_ATTACHMENT_USAGE_COLOR, 0, PB::PB_TEXTURE_FORMAT_B8G8R8A8_UNORM } },
		};

		rpDesc.m_attachments = attachments;
		rpDesc.m_attachmentCount = _countof(attachments);
		rpDesc.m_subpasses = subpassDescs;
		rpDesc.m_subpassCount = _countof(subpassDescs);

		auto renderPass = m_renderer->GetRenderPassCache()->GetRenderPass(rpDesc);

		PB::PipelineDesc pipelineDesc;
		pipelineDesc.m_renderPass = renderPass;
		pipelineDesc.m_subpass = 0;
		pipelineDesc.m_renderArea = { 0, 0, m_swapchain->GetWidth(), m_swapchain->GetHeight() };
		pipelineDesc.m_shaderModules[PB::PB_SHADER_STAGE_VERTEX] = vertModule;
		pipelineDesc.m_shaderModules[PB::PB_SHADER_STAGE_FRAGMENT] = fragModule;
		auto pipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);

		//DynamicArray<PB::Float4, 1> clearColors = { { 0.0f, 0.4f, 0.6f, 0.0f } };
		CLib::Vector<PB::Float4, 1> clearColors = { { 0.0f, 0.0f, 0.0f, 0.0f } };

		PB::CommandContextDesc contextDesc;
		contextDesc.m_renderer = m_renderer;
		PB::SCommandContext cmdContext(m_renderer);
		cmdContext->Init(contextDesc);
		
		cmdContext->Begin();

		PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
		auto* swapChainTex = m_swapchain->GetImage(swapChainIdx);

		cmdContext->CmdTransitionTexture(swapChainTex, PB::PB_TEXTURE_STATE_COLORTARGET);
		cmdContext->CmdBeginRenderPass(renderPass, m_swapchain->GetWidth(), m_swapchain->GetHeight(), &swapchainTextureViews[swapChainIdx], 1, clearColors.Data(), clearColors.Count() );
		cmdContext->CmdBindPipeline(pipeline);
		cmdContext->CmdDraw(3);
		cmdContext->CmdEndRenderPass();
		cmdContext->CmdTransitionTexture(swapChainTex, PB::PB_TEXTURE_STATE_PRESENT);
		cmdContext->End();
		cmdContext->Return();

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

	m_renderer->WaitIdle();
	m_renderer->FreeTexture(testTex);
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
