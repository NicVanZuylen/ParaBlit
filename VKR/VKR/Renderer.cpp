#define VK_USE_PLATFORM_WIN32_KHR
#include "Renderer.h"
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"

namespace PB 
{
	Renderer::Renderer()
	{
		for (u32 i = 0; i < PB_FRAME_IN_FLIGHT_COUNT; ++i)
			m_frameInfos.Push(FrameInfo());
	}

	Renderer::~Renderer()
	{
		vkDeviceWaitIdle(m_device.GetHandle());

		m_swapchain.Destroy();
		if (m_windowSurface)
			vkDestroySurfaceKHR(m_vkInstance.GetHandle(), m_windowSurface, nullptr);

		m_pipelineCache.Destroy();
		m_renderPassCache.Destroy();
		m_framebufferCache.Destroy();
		m_shaderModuleCache.Destroy();
		m_viewCache.Destroy();

		// Destroy sync objects.
		for (u32 i = 0; i < m_frameInfos.Count(); ++i)
		{
			vkDestroyFence(m_device.GetHandle(), m_frameInfos[i].m_frameFence, nullptr);
			vkDestroySemaphore(m_device.GetHandle(), m_frameInfos[i].m_imageAquireSempahore, nullptr);
			vkDestroySemaphore(m_device.GetHandle(), m_frameInfos[i].m_frameSemaphore, nullptr);
		}
		m_frameInfos.SetCount(0);

		// Destroy master command pool (will free master command buffers).
		vkDestroyCommandPool(m_device.GetHandle(), m_masterCmdPool, nullptr);
	}

	void Renderer::Init(const RendererDesc& desc)
	{
		m_vkInstance.Create(desc.m_extensionNames, desc.m_extensionCount);
		m_device.Init(m_vkInstance.GetHandle());
		m_renderPassCache.Init(&m_device);
		m_viewCache.Init(&m_device);
		m_framebufferCache.Init(&m_device);
		m_shaderModuleCache.Init(&m_device);
		m_pipelineCache.Init(&m_device);

		CreateWindowSurface(desc.m_windowInfo);
		CreateSyncObjects();
		CreateCmdBuffers();

		vkGetDeviceQueue(m_device.GetHandle(), m_device.GetGraphicsQueueFamilyIndex(), 0, &m_presentQueue);
		PB_ASSERT(m_presentQueue);
	}

	ISwapChain* Renderer::CreateSwapChain(const SwapChainDesc& desc)
	{
		m_swapchainDesc = desc; // Cache desc for swap chain re-creation if swap-chain is lost/outdated.
		m_swapchain.Init(m_swapchainDesc, this, m_windowSurface);
		return reinterpret_cast<ISwapChain*>(&m_swapchain);
	}

	Device* Renderer::GetDevice()
	{
		return &m_device;
	}

	IRenderPassCache* Renderer::GetRenderPassCache()
	{
		return &m_renderPassCache;
	}

	ITextureViewCache* Renderer::GetTextureViewCache()
	{
		return &m_viewCache;
	}

	IShaderModuleCache* Renderer::GetShaderModuleCache()
	{
		return &m_shaderModuleCache;
	}

	IPipelineCache* Renderer::GetPipelineCache()
	{
		return &m_pipelineCache;
	}

	FramebufferCache* Renderer::GetFramebufferCache()
	{
		return &m_framebufferCache;
	}

	VkCommandBuffer Renderer::AllocateCommandBuffer()
	{
		std::lock_guard<std::mutex> lock(m_contextCmdAllocLock); // Only one thread should be allocating command context buffers at any given time.

		if (m_freeContextCmdBuffers.Count() > 0) // Return an available existing command buffer if possible.
		{
			VkCommandBuffer nextAvailable = m_freeContextCmdBuffers[m_freeContextCmdBuffers.Count() - 1];
			m_freeContextCmdBuffers.Pop();
			return nextAvailable;
		}

		// ...Otherwise allocate a new one.
		VkCommandBuffer newCmdBuf;
		VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
		allocInfo.commandBufferCount = 1;
		allocInfo.commandPool = m_masterCmdPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		vkAllocateCommandBuffers(m_device.GetHandle(), &allocInfo, &newCmdBuf);
		PB_ASSERT(newCmdBuf);
		return newCmdBuf;
	}

	void Renderer::ReturnCommandBuffer(CommandContext& context)
	{
		std::lock_guard<std::mutex> lock(m_contextCmdReturnLock); // Only one thread should be returning command context buffers at any given time.

		if(context.GetIsInternal())
			m_internalCmdBuffers.Push(context.GetCmdBuffer());
		else if (context.GetIsPriority())
			m_frameInfos[m_curFrameInfoIdx].m_prioritySubmittedContextBuffers.Push(context.GetCmdBuffer());
		else
			m_frameInfos[m_curFrameInfoIdx].m_submittedContextCmdBuffers.Push(context.GetCmdBuffer());
		context.Invalidate(); // Invalidate internal command buffer.
	}

	void Renderer::ReturnOpenBuffer(CommandContext& context)
	{
		std::lock_guard<std::mutex> lock(m_contextCmdReturnLock);

		m_freeContextCmdBuffers.Push(context.GetCmdBuffer());
		context.Invalidate();
	}

	void Renderer::BeginFrame()
	{
		if (m_curFrameInfoIdx < m_swapchain.GetImageCount() - 1)
			++m_curFrameInfoIdx;
		else
			m_curFrameInfoIdx = 0;

		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];
		m_freeContextCmdBuffers += curFrameInfo.m_enqueuedCmdBuffers; // Append submitted command buffers to free buffer array, as they can now be modified.
		curFrameInfo.m_submittedInternalCmdBuffers.Clear();
		curFrameInfo.m_prioritySubmittedContextBuffers.Clear();
		curFrameInfo.m_submittedContextCmdBuffers.Clear();
		for (auto& stagingBuffer : curFrameInfo.m_stagingBuffers)
			stagingBuffer.Destroy(); // TODO: We should probably find a way to re-use these instead of destroying them.
		curFrameInfo.m_stagingBuffers.Clear();
		PB_ASSERT(curFrameInfo.m_state == PB_FRAME_STATE_IN_FLIGHT || curFrameInfo.m_state == PB_FRAME_STATE_OPEN);

		// TODO: Optimization: The wait and aquire process should be moved to EndFrame() as all CPU-side code for this frame between here and EndFrame() is halted until the last frame with this index has completed GPU-side execution.

		// Wait for frame to finish if it's still in-flight.
		vkWaitForFences(m_device.GetHandle(), 1, &curFrameInfo.m_frameFence, VK_TRUE, ~(0ULL));
		vkResetFences(m_device.GetHandle(), 1, &curFrameInfo.m_frameFence);

		curFrameInfo.m_state = PB_FRAME_STATE_OPEN;

		PB_ASSERT(curFrameInfo.m_frameSemaphore);
		PB_ERROR_CHECK(vkAcquireNextImageKHR(m_device.GetHandle(), m_swapchain.GetHandle(), ~(0ULL), curFrameInfo.m_imageAquireSempahore, VK_NULL_HANDLE, &curFrameInfo.m_presentImageIdx), "Failed to acquire next swapchain image.");
	}

	void Renderer::EndFrame()
	{
		InlineContextCmdBuffers();
		SubmitFrame();
		Present();
		++m_currentFrame;
	}

	void Renderer::WaitIdle()
	{
		vkDeviceWaitIdle(m_device.GetHandle());
	}

	u32 Renderer::GetCurrentSwapchainImageIndex()
	{
		return m_frameInfos[m_curFrameInfoIdx].m_presentImageIdx;
	}

	CmdContextPool& Renderer::GetContextPool()
	{
		return m_contextPool;
	}

	u64 Renderer::GetCurrentFrame()
	{
		return m_currentFrame;
	}

	void Renderer::CreateWindowSurface(WindowDesc* windowInfo)
	{
#ifdef PARABLIT_WINDOWS
		VkWin32SurfaceCreateInfoKHR surfaceInfo =
		{
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			windowInfo->m_instance,
			windowInfo->m_handle
		};

		PB_ERROR_CHECK(vkCreateWin32SurfaceKHR(m_vkInstance.GetHandle(), &surfaceInfo, nullptr, &m_windowSurface));
		PB_ASSERT(m_windowSurface);
#endif
	}

	void Renderer::CreateSyncObjects()
	{
		VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT }; // Create fences already signalled, to indicate frames are not in-flight.
		VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };

		m_frameInfos.SetCount(PB_FRAME_IN_FLIGHT_COUNT);
		for (u32 i = 0; i < PB_FRAME_IN_FLIGHT_COUNT; ++i)
		{
			FrameInfo& frameInfo = m_frameInfos[i];
			vkCreateFence(m_device.GetHandle(), &fenceInfo, nullptr, &frameInfo.m_frameFence);
			PB_ASSERT(frameInfo.m_frameFence);
			vkCreateSemaphore(m_device.GetHandle(), &semaphoreInfo, nullptr, &frameInfo.m_imageAquireSempahore);
			PB_ASSERT(frameInfo.m_imageAquireSempahore);
			vkCreateSemaphore(m_device.GetHandle(), &semaphoreInfo, nullptr, &frameInfo.m_frameSemaphore);
			PB_ASSERT(frameInfo.m_frameSemaphore);
		}
	}

	void Renderer::CreateCmdBuffers()
	{
		VkCommandPoolCreateInfo cmdPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		cmdPoolInfo.queueFamilyIndex = m_device.GetGraphicsQueueFamilyIndex();
		
		PB_ERROR_CHECK(vkCreateCommandPool(m_device.GetHandle(), &cmdPoolInfo, nullptr, &m_masterCmdPool), "Failed to create master command pool.");
		PB_ASSERT(m_masterCmdPool);

		// Allocate and assign master command buffers to frame infos.
		VkCommandBufferAllocateInfo cmdAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
		cmdAllocInfo.commandBufferCount = m_frameInfos.Count();
		cmdAllocInfo.commandPool = m_masterCmdPool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		m_masterCmdBuffers.SetCount(m_frameInfos.Count());
		PB_ERROR_CHECK(vkAllocateCommandBuffers(m_device.GetHandle(), &cmdAllocInfo, m_masterCmdBuffers.Data()));

		for (u32 i = 0; i < m_frameInfos.Count(); ++i)
			m_frameInfos[i].m_masterCommandBuffer = m_masterCmdBuffers[i];
	}

	void Renderer::InlineContextCmdBuffers()
	{
		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];

		// Begin master command buffer recording.
		//VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
		//beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		//beginInfo.pInheritanceInfo = nullptr;

		//vkBeginCommandBuffer(curFrameInfo.m_masterCommandBuffer, &beginInfo);

		//// Batch-inline internal command buffers first.
		//if (m_internalCmdBuffers.Count() > 0)
		//	vkCmdExecuteCommands(curFrameInfo.m_masterCommandBuffer, m_internalCmdBuffers.Count(), m_internalCmdBuffers.Data());

		//curFrameInfo.m_submittedInternalCmdBuffers += m_internalCmdBuffers;
		//m_internalCmdBuffers.Clear();

		//// Batch-inline priority command buffers second.
		//if(curFrameInfo.m_prioritySubmittedContextBuffers.Count() > 0)
		//	vkCmdExecuteCommands(curFrameInfo.m_masterCommandBuffer, curFrameInfo.m_prioritySubmittedContextBuffers.Count(), curFrameInfo.m_prioritySubmittedContextBuffers.Data());

		//// Batch-inline command buffers.
		//if (curFrameInfo.m_submittedContextCmdBuffers.Count() > 0)
		//	vkCmdExecuteCommands(curFrameInfo.m_masterCommandBuffer, curFrameInfo.m_submittedContextCmdBuffers.Count(), curFrameInfo.m_submittedContextCmdBuffers.Data());

		//// End master command buffer recording.
		//vkEndCommandBuffer(curFrameInfo.m_masterCommandBuffer);
		
	}

	void Renderer::SubmitFrame()
	{
		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];

		curFrameInfo.m_submittedInternalCmdBuffers += m_internalCmdBuffers;
		m_internalCmdBuffers.Clear();

		curFrameInfo.m_enqueuedCmdBuffers.Clear();
		curFrameInfo.m_enqueuedCmdBuffers += curFrameInfo.m_submittedInternalCmdBuffers;
		curFrameInfo.m_enqueuedCmdBuffers += curFrameInfo.m_prioritySubmittedContextBuffers;
		curFrameInfo.m_enqueuedCmdBuffers += curFrameInfo.m_submittedContextCmdBuffers;

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
		//submitInfo.commandBufferCount = 1;
		//submitInfo.pCommandBuffers = &curFrameInfo.m_masterCommandBuffer;
		submitInfo.commandBufferCount = curFrameInfo.m_enqueuedCmdBuffers.Count();
		submitInfo.pCommandBuffers = curFrameInfo.m_enqueuedCmdBuffers.Data();
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &curFrameInfo.m_frameSemaphore;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &curFrameInfo.m_imageAquireSempahore;
		
		// Wait for color attachment output before signalling the frame semaphore.
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.pWaitDstStageMask = waitStages;

		// Signal the frame fence when frame commands have finished execution.
		PB_ERROR_CHECK(vkQueueSubmit(m_presentQueue, 1, &submitInfo, curFrameInfo.m_frameFence), "Failed to submit master frame command buffer.");
	}

	void Renderer::Present()
	{
		PB_ASSERT(m_curFrameInfoIdx != m_lastFrameInfoIdx, "Must call BeginFrame() before EndFrame().");

		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];

		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = m_swapchain.GetHandlePtr();
		presentInfo.pResults = nullptr;
		presentInfo.pWaitSemaphores = &curFrameInfo.m_frameSemaphore;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &curFrameInfo.m_presentImageIdx;

		PB_ERROR_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo), "Failed to present swapchain image.");

		curFrameInfo.m_state = PB_FRAME_STATE_IN_FLIGHT;
	}
}
