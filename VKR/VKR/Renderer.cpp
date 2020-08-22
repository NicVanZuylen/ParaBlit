#define VK_USE_PLATFORM_WIN32_KHR
#include "Renderer.h"
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"
#include "PBUtil.h"

namespace PB 
{
	static CLib::Allocator vectorAllocator;

	static void* VectorAlloc(unsigned long long size)
	{
		return vectorAllocator.Alloc((u32)size);
	}

	static void VectorFree(void* ptr)
	{
		vectorAllocator.Free(ptr);
	}

	Renderer::Renderer()
	{
		CLib::vectorAllocFunc = VectorAlloc;
		CLib::vectorFreeFunc = VectorFree;

		for (u32 i = 0; i < PB_FRAME_IN_FLIGHT_COUNT; ++i)
		{
			new (&m_frameInfos.PushBack()) FrameInfo();
		}
	}

	Renderer::~Renderer()
	{
		vkDeviceWaitIdle(m_device.GetHandle());

		m_swapchain.Destroy();
		if (m_windowSurface)
			vkDestroySurfaceKHR(m_vkInstance.GetHandle(), m_windowSurface, nullptr);

		// Destroy sync objects.
		for (u32 i = 0; i < m_frameInfos.Count(); ++i)
		{
			// Destroy frame DRI buffers.
			for (auto& driBuffer : m_frameInfos[i].m_driBuffers)
			{
				driBuffer.m_buffer.Destroy();
				driBuffer.m_stagingBuffer.Destroy();
				driBuffer.m_descSet = VK_NULL_HANDLE;
				driBuffer.~DRIBuffer();
			}

			vkDestroyFence(m_device.GetHandle(), m_frameInfos[i].m_frameFence, nullptr);
			vkDestroySemaphore(m_device.GetHandle(), m_frameInfos[i].m_imageAquireSempahore, nullptr);
			vkDestroySemaphore(m_device.GetHandle(), m_frameInfos[i].m_frameSemaphore, nullptr);

			m_frameInfos[i].~FrameInfo();
		}
		m_frameInfos.SetCount(0);

		m_pipelineCache.Destroy();
		m_renderPassCache.Destroy();
		m_framebufferCache.Destroy();
		m_shaderModuleCache.Destroy();

		m_masterResourceDescSet = VK_NULL_HANDLE;
		m_masterSetLayout = VK_NULL_HANDLE;
		m_viewCache.Destroy();

		// Destroy DRI buffer set layout and descriptor pool.
		vkDestroyDescriptorSetLayout(m_device.GetHandle(), m_driSetLayout, nullptr);
		m_driSetLayout = VK_NULL_HANDLE;

		// Destroy UBO set layout.
		vkDestroyDescriptorSetLayout(m_device.GetHandle(), m_uboSetLayout, nullptr);
		m_uboSetLayout = VK_NULL_HANDLE;

		vkDestroyDescriptorPool(m_device.GetHandle(), m_sharedDescPool, nullptr);
		m_sharedDescPool = VK_NULL_HANDLE;

		// Destroy master command pool (will free master command buffers).
		vkDestroyCommandPool(m_device.GetHandle(), m_masterCmdPool, nullptr);
	}

	void Renderer::Init(const RendererDesc& desc)
	{
		m_vkInstance.Create(desc.m_extensionNames, desc.m_extensionCount);
		m_device.Init(m_vkInstance.GetHandle());
		m_renderPassCache.Init(&m_device);
		m_viewCache.Init(&m_device, &m_masterResourceDescSet, &m_masterSetLayout);
		m_framebufferCache.Init(&m_device);
		m_shaderModuleCache.Init(&m_device);

		// Needed for pipelines, so we create these before the pipeline cache.
		CreatePoolAndSetLayouts();
		for (u32 i = 0; i < PB_FRAME_IN_FLIGHT_COUNT; ++i)
		{
			DRIBuffer* newBuf = new(&m_frameInfos[i].m_driBuffers.PushBack()) DRIBuffer();
			CreateDRIBuffer(*newBuf);
		}

		m_pipelineCache.Init(this);

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

	ViewCache* Renderer::GetViewCache()
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
			return m_freeContextCmdBuffers.PopBack();
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
			m_internalCmdBuffers.PushBack(context.GetCmdBuffer());
		else if (context.GetIsPriority())
			m_frameInfos[m_curFrameInfoIdx].m_prioritySubmittedContextBuffers.PushBack(context.GetCmdBuffer());
		else
			m_frameInfos[m_curFrameInfoIdx].m_submittedContextCmdBuffers.PushBack(context.GetCmdBuffer());
		context.Invalidate(); // Invalidate internal command buffer.
	}

	void Renderer::ReturnOpenBuffer(CommandContext& context)
	{
		std::lock_guard<std::mutex> lock(m_contextCmdReturnLock);

		m_freeContextCmdBuffers.PushBack(context.GetCmdBuffer());
		context.Invalidate();
	}

	void Renderer::EndFrame()
	{
		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];

		// Wait for frame to finish if it's still in-flight.
		vkWaitForFences(m_device.GetHandle(), 1, &curFrameInfo.m_frameFence, VK_TRUE, ~(0ULL));
		vkResetFences(m_device.GetHandle(), 1, &curFrameInfo.m_frameFence);

		m_freeContextCmdBuffers += curFrameInfo.m_enqueuedCmdBuffers; // Append submitted command buffers to free buffer array, as they are now no longer in-flight.
		curFrameInfo.m_state = PB_FRAME_STATE_OPEN;

		m_uboDescSets += curFrameInfo.m_submittedUBODescSets;
		curFrameInfo.m_submittedUBODescSets.Clear();

		PB_ASSERT(curFrameInfo.m_frameSemaphore);
		PB_ERROR_CHECK(vkAcquireNextImageKHR(m_device.GetHandle(), m_swapchain.GetHandle(), ~(0ULL), curFrameInfo.m_imageAquireSempahore, VK_NULL_HANDLE, &curFrameInfo.m_presentImageIdx));

		InlineContextCmdBuffers();
		SubmitFrame();
		Present();
		BeginNextFrame();
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

	IBufferObject* Renderer::AllocateBuffer(const BufferObjectDesc& bufDesc)
	{
		auto* internalBuf = m_allocator.Alloc<BufferObject>();
		internalBuf->Create(this, bufDesc);
		return reinterpret_cast<IBufferObject*>(internalBuf);
	}

	void Renderer::FreeBuffer(IBufferObject* buffer)
	{
		auto* internalBuf = reinterpret_cast<BufferObject*>(buffer);
		internalBuf->Destroy();
		m_allocator.Free(internalBuf);
	}

	ITexture* Renderer::AllocateTexture(const TextureDesc& texDesc)
	{
		auto* internalTex = m_allocator.Alloc<Texture>();
		internalTex->Create(this, texDesc);
		return reinterpret_cast<ITexture*>(internalTex);
	}

	void Renderer::FreeTexture(ITexture* texture)
	{
		auto* internalTex = reinterpret_cast<Texture*>(texture);
		internalTex->Destroy();
		m_allocator.Free(internalTex);
	}

	Sampler Renderer::GetSampler(const SamplerDesc& samplerDesc)
	{
		return m_viewCache.GetSampler(samplerDesc);
	}

	CmdContextPool& Renderer::GetContextPool()
	{
		return m_contextPool;
	}

	DRIBuffer* Renderer::GetDRIBuffer()
	{
		// TODO: These buffers should really be moved to a frame info when they are allocated, they are currently shared across multiple frames in flight.

		// Find the first non-full buffer, otherwise create a new one.
		auto& driBuffers = m_frameInfos[m_curFrameInfoIdx].m_driBuffers;
		for (auto& buffer : driBuffers)
		{
			if (!buffer.m_isFull)
				return &buffer;
		}

		CreateDRIBuffer(driBuffers.PushBack());
		return &driBuffers.Back();
	}

	VkDescriptorSetLayout Renderer::GetDRISetLayout()
	{
		return m_driSetLayout;
	}

	VkDescriptorSet Renderer::GetMasterSet()
	{
		return m_masterResourceDescSet;
	}

	VkDescriptorSetLayout Renderer::GetMasterSetLayout()
	{
		return m_masterSetLayout;
	}

	VkDescriptorSet Renderer::GetUBOSet()
	{
		if (m_uboDescSets.Count() > 0)
		{
			m_usedUBODescSets.PushBack(m_uboDescSets.PopBack());
			return m_usedUBODescSets.Back();
		}

		VkDescriptorSetAllocateInfo uboSetAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
		uboSetAllocInfo.descriptorPool = m_sharedDescPool;
		uboSetAllocInfo.descriptorSetCount = 1;
		uboSetAllocInfo.pSetLayouts = &m_uboSetLayout;
		uboSetAllocInfo.pNext = nullptr;
		
		VkDescriptorSet& descSet = m_usedUBODescSets.PushBack();
		PB_ERROR_CHECK(vkAllocateDescriptorSets(m_device.GetHandle(), &uboSetAllocInfo, &descSet));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(descSet);
		return descSet;
	}

	VkDescriptorSetLayout Renderer::GetUBOSetLayout()
	{
		return m_uboSetLayout;
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
		
		PB_ERROR_CHECK(vkCreateCommandPool(m_device.GetHandle(), &cmdPoolInfo, nullptr, &m_masterCmdPool));
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

	void Renderer::CreatePoolAndSetLayouts()
	{
		CLib::Vector<VkDescriptorPoolSize, 2> poolSizes;

		VkDescriptorPoolSize& driPoolSize = poolSizes.PushBack();
		driPoolSize.descriptorCount = MaxDRIBufferCount;
		driPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		VkDescriptorPoolSize& uboPoolSize = poolSizes.PushBack();
		uboPoolSize.descriptorCount = (m_device.GetDescriptorIndexingProperties()->maxPerStageDescriptorUpdateAfterBindUniformBuffers - 1) * maxUBOSets;
		uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		VkDescriptorPoolCreateInfo sharedPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr };
		sharedPoolInfo.flags = 0;
		sharedPoolInfo.maxSets = MaxDRIBufferCount * PB_FRAME_IN_FLIGHT_COUNT;

		sharedPoolInfo.poolSizeCount = poolSizes.Count();
		sharedPoolInfo.pPoolSizes = poolSizes.Data();

		PB_ERROR_CHECK(vkCreateDescriptorPool(m_device.GetHandle(), &sharedPoolInfo, nullptr, &m_sharedDescPool));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_sharedDescPool);

		VkDescriptorSetLayoutBinding driLayoutBinding;
		driLayoutBinding.binding = 0;
		driLayoutBinding.descriptorCount = 1;
		driLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		driLayoutBinding.pImmutableSamplers = nullptr;
		driLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding uboLayoutBinding;
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorCount = m_device.GetDescriptorIndexingProperties()->maxPerStageDescriptorUpdateAfterBindUniformBuffers - 1;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.pImmutableSamplers = nullptr;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		// UBO sets may only be partially bound.
		VkDescriptorBindingFlags uboBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
		VkDescriptorSetLayoutBindingFlagsCreateInfo uboFlags{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr };
		uboFlags.bindingCount = 1;
		uboFlags.pBindingFlags = &uboBindingFlags;

		VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr };
		layoutInfo.flags = 0;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &driLayoutBinding;
		layoutInfo.pNext = &uboFlags;

		// DRI set layout
		PB_ERROR_CHECK(vkCreateDescriptorSetLayout(m_device.GetHandle(), &layoutInfo, nullptr, &m_driSetLayout));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_driSetLayout);

		// UBO set layout
		layoutInfo.pBindings = &uboLayoutBinding;
		PB_ERROR_CHECK(vkCreateDescriptorSetLayout(m_device.GetHandle(), &layoutInfo, nullptr, &m_uboSetLayout));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_uboSetLayout);
	}

	void Renderer::CreateDRIBuffer(DRIBuffer& buffer)
	{
		PB_ASSERT_MSG(DRIBufferSize <= m_device.GetDeviceLimits()->maxUniformBufferRange, "Device does not support large enough uniform buffers.");

		BufferObjectDesc driBufferObjDesc;
		driBufferObjDesc.m_bufferSize = DRIBufferSize;
		driBufferObjDesc.m_options = 0;
		driBufferObjDesc.m_usage = PB_BUFFER_USAGE_COPY_DST | PB_BUFFER_USAGE_UNIFORM;
		buffer.m_buffer.Create(this, driBufferObjDesc);

		driBufferObjDesc.m_usage = PB_BUFFER_USAGE_COPY_SRC;
		driBufferObjDesc.m_options = PB_BUFFER_OPTION_CPU_ACCESSIBLE;
		buffer.m_stagingBuffer.Create(this, driBufferObjDesc);

		// Allocate the descriptor set for the DRI buffer.
		VkDescriptorSetAllocateInfo driAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
		driAllocInfo.descriptorPool = m_sharedDescPool;
		driAllocInfo.descriptorSetCount = 1;
		driAllocInfo.pSetLayouts = &m_driSetLayout;

		PB_ERROR_CHECK(vkAllocateDescriptorSets(m_device.GetHandle(), &driAllocInfo, &buffer.m_descSet));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(buffer.m_descSet);

		// Write the buffer to it's descriptor set.
		VkDescriptorBufferInfo driBufferInfo;
		driBufferInfo.buffer = buffer.m_buffer.GetHandle();
		driBufferInfo.offset = 0;
		driBufferInfo.range = DRIBufferSize;

		VkWriteDescriptorSet descWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
		descWrite.descriptorCount = 1;
		descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descWrite.dstArrayElement = 0;
		descWrite.dstBinding = 0;
		descWrite.dstSet = buffer.m_descSet;
		descWrite.pBufferInfo = &driBufferInfo;
		descWrite.pImageInfo = nullptr;
		descWrite.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_device.GetHandle(), 1, &descWrite, 0, nullptr);
	}

	void Renderer::BeginNextFrame()
	{
		if (m_curFrameInfoIdx < m_swapchain.GetImageCount() - 1)
			++m_curFrameInfoIdx;
		else
			m_curFrameInfoIdx = 0;

		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];
		curFrameInfo.m_submittedInternalCmdBuffers.Clear();
		curFrameInfo.m_prioritySubmittedContextBuffers.Clear();
		curFrameInfo.m_submittedContextCmdBuffers.Clear();
		for (auto& stagingBuffer : curFrameInfo.m_stagingBuffers)
			stagingBuffer.Destroy(); // TODO: We should probably find a way to re-use these instead of destroying them.
		curFrameInfo.m_stagingBuffers.Clear();
		PB_ASSERT(curFrameInfo.m_state == PB_FRAME_STATE_IN_FLIGHT || curFrameInfo.m_state == PB_FRAME_STATE_OPEN);
	}

	void Renderer::InlineContextCmdBuffers()
	{
		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];

		if (curFrameInfo.m_driBuffers[0].m_currentOffset > 0)
		{
			// Make an internal command buffer to copy DRI buffers before rendering uses them.
			const VkCommandBuffer& driCopyCommandBuffer = m_internalCmdBuffers.PushBack(AllocateCommandBuffer());

			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			beginInfo.pInheritanceInfo = nullptr;

			vkBeginCommandBuffer(driCopyCommandBuffer, &beginInfo);

			VkBufferCopy copyRegion{ 0, 0, 0 };
			for (auto& driBuffer : curFrameInfo.m_driBuffers)
			{
				if (driBuffer.m_currentOffset == 0)
					continue;
				copyRegion.size = driBuffer.m_currentOffset * sizeof(int);
				vkCmdCopyBuffer(driCopyCommandBuffer, driBuffer.m_stagingBuffer.GetHandle(), driBuffer.m_buffer.GetHandle(), 1, &copyRegion);

				driBuffer.m_currentOffset = 0;
				driBuffer.m_isFull = false;
			}

			vkEndCommandBuffer(driCopyCommandBuffer);
		}

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

		curFrameInfo.m_submittedUBODescSets += m_usedUBODescSets;
		m_usedUBODescSets.Clear();

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
		VkResult error = VK_SUCCESS;
		PB_ERROR_CHECK(error = vkQueueSubmit(m_presentQueue, 1, &submitInfo, curFrameInfo.m_frameFence));
		PB_BREAK_ON_ERROR;
	}

	void Renderer::Present()
	{
		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];

		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = m_swapchain.GetHandlePtr();
		presentInfo.pResults = nullptr;
		presentInfo.pWaitSemaphores = &curFrameInfo.m_frameSemaphore;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &curFrameInfo.m_presentImageIdx;

		PB_ERROR_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo));

		curFrameInfo.m_state = PB_FRAME_STATE_IN_FLIGHT;
	}
}
