#include "PipelineApplication.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cassert>
#include <iostream>
#include <filesystem>

#include "Encode/SpirvShaderEncoder.h"
#include "Encode/MeshEncoder.h"
#include "Encode/Texture2DEncoder.h"

#include "Engine.Control/ISettingsParsers.h"

void* GetWindowHandle(GLFWwindow* window)
{
#ifdef GLFW_EXPOSE_NATIVE_WIN32
	return (void*)glfwGetWin32Window(window);
#endif
}

void* GetWindowInstance()
{
	return (void*)GetModuleHandle(NULL);
}

namespace AssetPipeline
{
	PipelineApplication::PipelineApplication(int argc, char** argv)
	{
		Ctrl::ICommandLine* parser = Ctrl::ICommandLine::Create(argc, argv);
		const Ctrl::IDataContainer* commandLineData = parser->GetData();
		Ctrl::ISettingsHub* settings = Ctrl::ISettingsHub::GetOrCreate();
		settings->AddSettings(commandLineData);

		m_useDebugWindow = commandLineData->HasToken("DebugWindow");

		Ctrl::ICommandLine::Destroy(parser);

		PB::RendererDesc rendererDesc{};
		rendererDesc.m_extensionCount = 0;
		rendererDesc.m_extensionNames = nullptr;
		rendererDesc.m_windowInfo = nullptr;

		if (m_useDebugWindow)
		{
			assert(glfwInit() && "GLFW Could not be initialized!");

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			m_window = glfwCreateWindow(DebugWindowWidth, DebugWindowHeight, "AssetPipelineDebugWindow", nullptr, nullptr);
			assert(m_window != nullptr && "GLFW could not create debug window!");

			uint32_t extensionCount = 0;
			const char** extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);

			PB::WindowDesc windowInfo = { (HINSTANCE)GetWindowInstance(), (HWND)GetWindowHandle(m_window) };
			rendererDesc.m_extensionCount = extensionCount;
			rendererDesc.m_extensionNames = extensionNames;
			rendererDesc.m_windowInfo = &windowInfo;

			m_renderer = PB::CreateRenderer();
			m_renderer->Init(rendererDesc);

			PB::SwapChainDesc swapchainDesc{};
			swapchainDesc.m_width = DebugWindowWidth;
			swapchainDesc.m_height = DebugWindowHeight;
			swapchainDesc.m_presentMode = PB::EPresentMode::FIFO;
			swapchainDesc.m_imageCount = 3;
			m_swapChain = m_renderer->CreateSwapChain(swapchainDesc);
		}
		else
		{
			m_renderer = PB::CreateRenderer();
			m_renderer->Init(rendererDesc);
		}
	}

	PipelineApplication::~PipelineApplication()
	{
		printf("\nDestroying Renderer...\n");
		PB::DestroyRenderer(m_renderer);
		m_renderer = nullptr;
		m_swapChain = nullptr;

		if (m_useDebugWindow)
		{
			printf("\nDestroying Window...\n");
			glfwDestroyWindow(m_window);
			m_window = nullptr;

			glfwTerminate();
		}

		Ctrl::ISettingsHub::Destroy();

		printf("\nShutting down...\n");
	}

	void PipelineApplication::Run(int argc, char** argv)
	{
		Ctrl::ISettingsHub* settings = Ctrl::ISettingsHub::GetOrCreate();

		printf("Running Asset Pipeline...\n\n");

		std::string assetRootDir = settings->GetStringValue("Path.AssetsDir");

		std::string workingDir = std::filesystem::current_path().string();
		std::string pipelineDbDir = workingDir + "\\" + "PipelineAssets\\";
		std::string dbDir = workingDir + "\\" + assetRootDir;

		printf("Working Directory: %s\n", workingDir.c_str());
		printf("Pipeline Assets Directory: %s\n\n", pipelineDbDir.c_str());
		printf("Assets Directory: %s\n\n", dbDir.c_str());

		// ==========================================================================================================================
		// Pipline Dependency Encoding

		{
			AssetPipeline::SpirvShaderEncoder dependencyShaderEncoder("<Pipeline SPIR-V Encoder>", "pipelineShaders.adb", pipelineDbDir.c_str());
		}

		printf("Encoding Pipeline Assets...\n\n");

		printf("Initialising ParaBlit...\n\n");

		// ==========================================================================================================================
		// Asset Encoding

		printf("Commence Encoding...\n\n");

		{
			AssetPipeline::SpirvShaderEncoder encoder("[SPIR-V Encoder]", "shaders.adb", dbDir.c_str());
		}

		{
			AssetPipeline::MeshEncoder encoder("[Mesh Encoder]", "meshes.adb", dbDir.c_str());
		}

		{
			AssetPipeline::Texture2DEncoder encoder("[Texture2D Encoder]", "textures.adb", dbDir.c_str(), pipelineDbDir.c_str(), m_renderer);
		}

		// ==========================================================================================================================

		printf("\nBuild Complete.\n");
	}
}
