#pragma once
#include "IRenderer.h"
#include "ParaBlitApi.h"
#include "VulkanInstance.h"
#include "Device.h"
#include "Swapchain.h"
#include "Dequeue.h"
#include "CmdContextPool.h"

#include <mutex>

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
		DynamicArray<VkCommandBuffer, 8> m_submittedContextCmdBuffers;		// Command context command buffers submitted for this frame, these will be emptied when the frame is next ready for use.
		DynamicArray<VkCommandBuffer, 8> m_prioritySubmittedContextBuffers;	// Submitted context command buffers that will be executed before non-priority command buffers.
		DynamicArray<VkCommandBuffer, 8> m_submittedInternalCmdBuffers;		// Submitted context command buffers that will be executed before non-priority command buffers.

	};

	class Renderer : public IRenderer
	{
	public:

		PARABLIT_API Renderer();

		PARABLIT_API ~Renderer();

		PARABLIT_API void Init(const RendererDesc& desc) override;

		PARABLIT_API ISwapChain* CreateSwapChain(const SwapChainDesc& desc) override;

		PARABLIT_API Device* GetDevice();

		PARABLIT_API VkCommandBuffer AllocateCommandBuffer();

		PARABLIT_API void ReturnCommandBuffer(CommandContext& context);

		PARABLIT_API void ReturnOpenBuffer(CommandContext& context);

		PARABLIT_API void BeginFrame() override;

		PARABLIT_API void EndFrame() override;

		PARABLIT_API CmdContextPool& GetContextPool();

	private:

		PARABLIT_API inline void CreateWindowSurface(WindowDesc* windowHandle) override;

		PARABLIT_API inline void CreateSyncObjects();

		PARABLIT_API inline void CreateCmdBuffers();

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
		VkCommandPool m_masterCmdPool = VK_NULL_HANDLE;
		DynamicArray<FrameInfo, PB_FRAME_IN_FLIGHT_COUNT> m_frameInfos;
		DynamicArray<VkCommandBuffer, PB_FRAME_IN_FLIGHT_COUNT> m_masterCmdBuffers;
		DynamicArray<VkCommandBuffer, 64> m_freeContextCmdBuffers;
		DynamicArray<VkCommandBuffer, 32> m_internalCmdBuffers;
		std::mutex m_contextCmdAllocLock;
		std::mutex m_contextCmdReturnLock;
		CmdContextPool m_contextPool;
	};
}

