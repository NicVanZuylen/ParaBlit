#pragma once
#include "IRenderer.h"
#include "ParaBlitApi.h"
#include "VulkanInstance.h"
#include "Device.h"
#include "Swapchain.h"
#include "FixedArray.h"
#include "Dequeue.h"
#include "CommandContext.h"

#ifndef PB_FRAME_IN_FLIGHT_COUNT
#define PB_FRAME_IN_FLIGHT_COUNT 3
#endif // !PB_FRAME_IN_FLIGHT_COUNT

namespace PB 
{
	enum EFrameState
	{
		PB_FRAME_STATE_OPEN,
		PB_FRAME_STATE_RECORDING,
		PB_FRAME_STATE_IN_FLIGHT
	};

	struct FrameInfo // Stores the overall state of a single frame.
	{
		VkImage m_presentImage = VK_NULL_HANDLE;
		VkSemaphore m_imageAquireSempahore = VK_NULL_HANDLE;				// Signalled when the swap chain image has been aquired, rendering will wait on this.
		VkSemaphore m_frameSemaphore = VK_NULL_HANDLE;						// Signalled when the frame is complete. This semaphore will be waited on before the image is presented.
		VkFence m_frameFence = VK_NULL_HANDLE;								// This semaphore indicates if this frame is currently in-flight.
		VkCommandBuffer m_masterCommandBuffer = VK_NULL_HANDLE;				// Contains all recorded graphics commands for the frame.
		EFrameState m_state = PB_FRAME_STATE_OPEN;							// TODO: Evaluate if this state enum is even needed outside of validation.
		u32 m_presentImageIdx = 0;											// Swapchain image index.
		DynamicArray<VkCommandBuffer> m_submittedContextCmdBuffers;			// Command context command buffers submitted for this frame, these will be emptied when the frame is next ready for use.
		DynamicArray<VkCommandBuffer> m_prioritySubmittedContextBuffers;	// Submitted context command buffers that will be executed before non-priority command buffers.
	};

	class Renderer : public IRenderer
	{
	public:

		PARABLIT_API Renderer();

		PARABLIT_API ~Renderer();

		PARABLIT_API void Init(const RendererDesc& desc) override;

		PARABLIT_API void CreateSwapChain(const SwapChainDesc& desc) override;

		PARABLIT_API Device* GetDevice();

		PARABLIT_API VkCommandBuffer AllocateCommandBuffer();

		PARABLIT_API void ReturnCommandBuffer(VkCommandBuffer& cmdBuf, bool priority = false);

		PARABLIT_API CommandContext* GetResourceCmdContext();

		PARABLIT_API void BeginFrame() override;

		PARABLIT_API void EndFrame() override;

	private:

		PARABLIT_API inline void CreateWindowSurface(WindowDesc* windowHandle) override;

		PARABLIT_API inline void CreateSyncObjects();

		PARABLIT_API inline void CreateMasterCmdBuffers();

		PARABLIT_API inline void InlineContextCmdBuffers();

		PARABLIT_API inline void SubmitFrame();

		PARABLIT_API inline void Present();

		VulkanInstance m_vkInstance;
		Device m_device;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		SwapChainDesc m_swapchainDesc;
		Swapchain m_swapchain;

		// Frame State
		VkQueue m_presentQueue = VK_NULL_HANDLE;
		u8 m_curFrameInfoIdx = 0;
		u8 m_lastFrameInfoIdx = ~0;
		FixedArray<FrameInfo, PB_FRAME_IN_FLIGHT_COUNT> m_frameInfos;
		VkCommandPool m_masterCmdPool = VK_NULL_HANDLE;
		FixedArray<VkCommandBuffer, PB_FRAME_IN_FLIGHT_COUNT> m_masterCmdBuffers;
		DynamicArray<VkCommandBuffer> m_freeContextCmdBuffers; // TODO: Replace these command buffer arrays with fixed arrays, when functionality to expand fixed arrays is implemented.
	};
}

