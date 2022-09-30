#pragma once
#include "Engine.ParaBlit/IRenderer.h"

#include <cstdint>

struct GLFWwindow;

namespace AssetPipeline
{
	class PipelineApplication
	{
	public:

		PipelineApplication(int argc, char** argv);

		~PipelineApplication();

		void Run(int argc, char** argv);

	private:

		static constexpr uint32_t DebugWindowWidth = 800;
		static constexpr uint32_t DebugWindowHeight = 800;

		GLFWwindow* m_window = nullptr;
		PB::IRenderer* m_renderer = nullptr;
		PB::ISwapChain* m_swapChain = nullptr;
		bool m_useDebugWindow = false;
	};
};