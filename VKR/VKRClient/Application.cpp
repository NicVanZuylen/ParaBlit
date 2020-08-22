#include "Application.h"
#include "Input.h"
#include "ParaBlitLog.h"
#include "WindowHandle.h"
#include "ICommandContext.h"
#include "CLib/Vector.h"
#include "QuickIO.h"
#include "Camera.h"
#include "Mesh.h"

#include <iostream>
#include <chrono>
#include <string>

#include "glfw3.h"
#include "IRenderer.h"
#include "ITexture.h"
#include "glm/include/glm.hpp"
#include "gtc/matrix_transform.hpp"

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
	PB::TextureDesc depthTextureDesc;
	depthTextureDesc.m_data.m_data = nullptr;
	depthTextureDesc.m_data.m_size = 0;
	depthTextureDesc.m_data.m_format = PB::PB_TEXTURE_FORMAT_D24_UNORM_S8_UINT;
	depthTextureDesc.m_initialState = PB::PB_TEXTURE_STATE_COPY_DST;
	depthTextureDesc.m_usageStates = PB::PB_TEXTURE_STATE_DEPTHTARGET;
	depthTextureDesc.m_initOptions = PB::PB_TEXTURE_INIT_ZERO_INITIALIZE;
	depthTextureDesc.m_width = m_swapchain->GetWidth();
	depthTextureDesc.m_height = m_swapchain->GetHeight();
	PB::ITexture* depthTexture = m_renderer->AllocateTexture(depthTextureDesc);

	PB::TextureViewDesc depthViewDesc = {};
	depthViewDesc.m_texture = depthTexture;
	depthViewDesc.m_renderer = m_renderer;
	depthViewDesc.m_format = PB::PB_TEXTURE_FORMAT_D24_UNORM_S8_UINT;
	depthViewDesc.m_expectedState = PB::PB_TEXTURE_STATE_DEPTHTARGET;
	auto depthView = depthTexture->GetRenderTargetView(depthViewDesc);
	
	PB::ShaderModule vertModule(0);
	PB::ShaderModule fragModule(0);

	PB::BufferObjectDesc bufferDesc;
	bufferDesc.m_bufferSize = sizeof(glm::mat4) * 3;
	bufferDesc.m_options = PB::PB_BUFFER_OPTION_ZERO_INITIALIZE;
	bufferDesc.m_usage = PB::PB_BUFFER_USAGE_COPY_DST | PB::PB_BUFFER_USAGE_UNIFORM;
	PB::IBufferObject* testBuf = m_renderer->AllocateBuffer(bufferDesc);

	PB::BufferViewDesc bufViewDesc;
	bufViewDesc.m_buffer = testBuf;
	bufViewDesc.m_offset = 0;
	bufViewDesc.m_size = testBuf->GetSize();
	PB::BufferView bufView = testBuf->GetView(bufViewDesc);

	PB::SamplerDesc samplerDesc;
	PB::Sampler testSampler = m_renderer->GetSampler(samplerDesc);

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
		desc.m_texture = nullptr;
		desc.m_renderer = m_renderer;
		desc.m_format = PB::PB_TEXTURE_FORMAT_B8G8R8A8_UNORM;
		desc.m_expectedState = PB::PB_TEXTURE_STATE_COLORTARGET;
		swapchainTextureViews.PushBack(m_swapchain->GetImage(i)->GetRenderTargetView(desc));
	}

	Camera cam(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), 0.5f, 5.0f);

	Mesh* paintMesh = new Mesh(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_paint.obj");
	Mesh* detailsMesh = new Mesh(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_details.obj");
	Mesh* glassMesh = new Mesh(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_glass.obj");

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

		cam.Update(deltaTime, m_input, m_window);

		glm::mat4* bufferMatrices = (glm::mat4*)testBuf->BeginPopulate();

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
		testBuf->EndPopulate();

		PB::RenderPassDesc rpDesc;
		PB::AttachmentDesc attachments[] =
		{
			{ PB::PB_TEXTURE_FORMAT_B8G8R8A8_UNORM, PB::PB_TEXTURE_STATE_COLORTARGET, PB::PB_TEXTURE_STATE_COLORTARGET, PB::PB_ATTACHMENT_START_ACTION_CLEAR },
			{ PB::PB_TEXTURE_FORMAT_D24_UNORM_S8_UINT, PB::PB_TEXTURE_STATE_DEPTHTARGET, PB::PB_TEXTURE_STATE_DEPTHTARGET, PB::PB_ATTACHMENT_START_ACTION_CLEAR, false },
		};

		PB::SubpassDesc subpassDescs[1];

		subpassDescs[0].m_attachments[0] = { PB::PB_ATTACHMENT_USAGE_COLOR, 0, PB::PB_TEXTURE_FORMAT_B8G8R8A8_UNORM };
		subpassDescs[0].m_attachments[1] = { PB::PB_ATTACHMENT_USAGE_DEPTHSTENCIL, 1, PB::PB_TEXTURE_FORMAT_D24_UNORM_S8_UINT };

		rpDesc.m_attachments = attachments;
		rpDesc.m_attachmentCount = _countof(attachments);
		rpDesc.m_subpasses = subpassDescs;
		rpDesc.m_subpassCount = _countof(subpassDescs);

		auto renderPass = m_renderer->GetRenderPassCache()->GetRenderPass(rpDesc);

		PB::PipelineDesc pipelineDesc{};
		pipelineDesc.m_renderPass = renderPass;
		pipelineDesc.m_subpass = 0;
		pipelineDesc.m_renderArea = { 0, 0, m_swapchain->GetWidth(), m_swapchain->GetHeight() };
		pipelineDesc.m_shaderModules[PB::PB_SHADER_STAGE_VERTEX] = vertModule;
		pipelineDesc.m_shaderModules[PB::PB_SHADER_STAGE_FRAGMENT] = fragModule;
		pipelineDesc.m_depthCompareOP = PB::PB_COMPARE_OP_LEQUAL;

		auto& vertexDesc = pipelineDesc.m_vertexDesc;
		vertexDesc.vertexSize = sizeof(Vertex);
		vertexDesc.vertexAttributes[0] = PB::PB_VERTEX_ATTRIBUTE_FLOAT4;
		vertexDesc.vertexAttributes[1] = PB::PB_VERTEX_ATTRIBUTE_FLOAT4;
		vertexDesc.vertexAttributes[2] = PB::PB_VERTEX_ATTRIBUTE_FLOAT4;
		vertexDesc.vertexAttributes[3] = PB::PB_VERTEX_ATTRIBUTE_FLOAT2;
		auto pipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);

		CLib::Vector<PB::Float4, 2> clearColors = { { 0.0f, 0.0f, 0.0f, 0.0f } , { 1.0f, 1.0f, 1.0f, 1.0f } };

		PB::CommandContextDesc contextDesc{};
		contextDesc.m_renderer = m_renderer;
		PB::SCommandContext cmdContext(m_renderer);
		cmdContext->Init(contextDesc);
		
		cmdContext->Begin();

		PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
		auto* swapChainTex = m_swapchain->GetImage(swapChainIdx);

		PB::TextureView attachmentViews[2] = { swapchainTextureViews[swapChainIdx], depthView };

		PB::BindingLayout bindings{};
		bindings.m_bindingLocation = PB::PB_BINDING_LAYOUT_LOCATION_DEFAULT;
		bindings.m_bufferCount = 1;

		PB::BufferView bufferViews[1] = { bufView };
		bindings.m_buffers = bufferViews;

		cmdContext->CmdTransitionTexture(swapChainTex, PB::PB_TEXTURE_STATE_COLORTARGET);
		cmdContext->CmdTransitionTexture(depthTexture, PB::PB_TEXTURE_STATE_DEPTHTARGET);
		cmdContext->CmdBeginRenderPass(renderPass, m_swapchain->GetWidth(), m_swapchain->GetHeight(), attachmentViews, 2, clearColors.Data(), clearColors.Count() );
		cmdContext->CmdBindPipeline(pipeline);
		cmdContext->CmdBindResources(bindings);
		cmdContext->CmdBindVertexBuffer(paintMesh->GetVertexBuffer(), paintMesh->GetIndexBuffer(), PB::PB_INDEX_TYPE_UINT32);
		cmdContext->CmdDrawIndexed(paintMesh->IndexCount(), 1);
		cmdContext->CmdBindVertexBuffer(detailsMesh->GetVertexBuffer(), detailsMesh->GetIndexBuffer(), PB::PB_INDEX_TYPE_UINT32);
		cmdContext->CmdDrawIndexed(detailsMesh->IndexCount(), 1);
		cmdContext->CmdBindVertexBuffer(glassMesh->GetVertexBuffer(), glassMesh->GetIndexBuffer(), PB::PB_INDEX_TYPE_UINT32);
		cmdContext->CmdDrawIndexed(glassMesh->IndexCount(), 1);
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

	delete paintMesh;
	delete detailsMesh;
	delete glassMesh;

	m_renderer->FreeBuffer(testBuf);
	m_renderer->FreeTexture(depthTexture);
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
