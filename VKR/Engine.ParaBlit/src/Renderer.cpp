#define VK_USE_PLATFORM_WIN32_KHR
#include "Renderer.h"
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"
#include "PBUtil.h"
#include "BindingCache.h"

namespace PB 
{
	thread_local ThreadCommandContext Renderer::t_threadResourceInitializationCommandContext{};

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

		t_threadResourceInitializationCommandContext.ExplicitDestroy();

		for (auto& context : m_freeCommandContexts)
		{
			m_commandContextAllocator.Free(context);
		}
		m_freeCommandContexts.Clear();

		for (uint32_t i = 0; i < PB_FRAME_IN_FLIGHT_COUNT; ++i)
		{
			FrameInfo& frameInfo = m_frameInfos[i];
			PB_ASSERT(frameInfo.m_deferredDeletions.Count() == 0);
		}

		for (uint32_t i = 0; i < m_pendingDeletions.Count(); ++i)
		{
			m_pendingDeletions[i]->OnDelete(m_allocator);
		}
		m_pendingDeletions.Clear();

		m_swapchain.Destroy();
		if (m_windowSurface)
		{
			vkDestroySurfaceKHR(m_vkInstance.GetHandle(), m_windowSurface, nullptr);
			m_windowSurface = VK_NULL_HANDLE;
		}

		// Destroy sync objects.
		for (u32 i = 0; i < m_frameInfos.Count(); ++i)
		{
			vkDestroyFence(m_device.GetHandle(), m_frameInfos[i].m_frameFence, nullptr);
			vkDestroySemaphore(m_device.GetHandle(), m_frameInfos[i].m_imageAcquireSempahore, nullptr);
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

		// Destroy UBO set layout.
		vkDestroyDescriptorSetLayout(m_device.GetHandle(), m_uboSetLayout, nullptr);
		m_uboSetLayout = VK_NULL_HANDLE;

		vkDestroyDescriptorPool(m_device.GetHandle(), m_sharedDescPool, nullptr);
		m_sharedDescPool = VK_NULL_HANDLE;

		for (auto& pool : m_liveCommandPools)
		{
			m_freeCommandPools.PushBack(pool.second);
		}
		m_liveCommandPools.clear();

		// Destroy master command pool (will free master command buffers).
		//vkDestroyCommandPool(m_device.GetHandle(), m_masterCmdPool, nullptr);
		for (auto& pool : m_freeCommandPools)
		{
			vkDestroyCommandPool(m_device.GetHandle(), pool->m_pool, nullptr);
			m_allocator.Free(pool);
		}

		m_allocator.DumpMemoryLeaks();
	}

	void Renderer::Init(const RendererDesc& desc)
	{
		m_vkInstance.Create(desc.m_extensionNames, desc.m_extensionCount);
		m_device.Init(m_vkInstance.GetHandle(), this, desc.m_windowInfo != nullptr);
		m_renderPassCache.Init(&m_device);
		m_viewCache.Init(&m_device, &m_masterResourceDescSet, &m_masterSetLayout);
		m_framebufferCache.Init(&m_device);
		m_shaderModuleCache.Init(&m_device);

		// Needed for pipelines, so we create these before the pipeline cache.
		CreatePoolAndSetLayouts();

		m_pipelineCache.Init(this);

		if (desc.m_windowInfo != nullptr)
		{
			m_windowDesc = *desc.m_windowInfo;
			CreateWindowSurface(desc.m_windowInfo);
		}

		CreateSyncObjects();

		vkGetDeviceQueue(m_device.GetHandle(), m_device.GetGraphicsQueueFamilyIndex(), 0, &m_presentQueue);
		PB_ASSERT(m_presentQueue);
	}

	ISwapChain* Renderer::CreateSwapChain(const SwapChainDesc& desc)
	{
		PB_ASSERT_MSG(m_windowSurface != VK_NULL_HANDLE, "Cannot create a Swapchain without a valid window surface.");

		m_swapchainDesc = desc;
		m_swapchain.Init(desc, this, m_windowSurface);
		m_validSwapchain = true;
		return reinterpret_cast<ISwapChain*>(&m_swapchain);
	}

	void Renderer::RecreateSwapchain(const SwapChainDesc& desc, WindowDesc* windowDesc)
	{
		PB_ASSERT(m_validSwapchain == true);

		m_swapchainDesc = desc;
		m_windowDesc = *windowDesc;
		m_resetSwapchain = true;
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

	ISwapChain* Renderer::GetSwapchain()
	{
		return &m_swapchain;
	}

	const DeviceLimitations* Renderer::GetDeviceLimitations() const
	{
		return m_device.GetDeviceLimitations();
	}

	bool Renderer::HasValidSwapchain()
	{
		return m_swapchain.GetHandle() != VK_NULL_HANDLE;
	}

	IShaderModuleCache* Renderer::GetShaderModuleCache()
	{
		return &m_shaderModuleCache;
	}

	IPipelineCache* Renderer::GetPipelineCache()
	{
		return &m_pipelineCache;
	}

	IFramebufferCache* Renderer::GetFramebufferCache()
	{
		return &m_framebufferCache;
	}

	VkCommandBuffer Renderer::AllocateCommandBuffer(bool secondary)
	{
		std::lock_guard<std::mutex> lock(m_contextCmdAllocLock); // Only one thread should be allocating command context buffers at any given time.
		std::thread::id threadId = std::this_thread::get_id();

		CommandPoolInfo* foundPoolInfo = nullptr;
		auto poolIt = m_liveCommandPools.find(threadId);
		if (poolIt == m_liveCommandPools.end())
		{
			if (m_freeCommandPools.Count() > 0)
			{
				foundPoolInfo = m_freeCommandPools.PopBack();
				m_liveCommandPools.insert({ threadId, foundPoolInfo });
				PB_ASSERT(foundPoolInfo->m_allocatedCmdBufferCount == 0);
			}
			else
			{
				foundPoolInfo = m_allocator.Alloc<CommandPoolInfo>();
				m_liveCommandPools.insert({ threadId, foundPoolInfo });

				VkCommandPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
				poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
				poolCreateInfo.queueFamilyIndex = m_device.GetGraphicsQueueFamilyIndex();

				PB_ERROR_CHECK(vkCreateCommandPool(m_device.GetHandle(), &poolCreateInfo, nullptr, &foundPoolInfo->m_pool));
				PB_BREAK_ON_ERROR;
			}
		}
		else
		{
			foundPoolInfo = poolIt->second;
		}

		PB_ASSERT(foundPoolInfo != nullptr);

		CommandPoolInfo& poolInfo = *foundPoolInfo;
		++poolInfo.m_allocatedCmdBufferCount;

		if (secondary == true)
		{
			if (poolInfo.m_freeSecondaryCommandBuffers.Count() > 0)
			{
				VkCommandBuffer cmdBuf = poolInfo.m_freeSecondaryCommandBuffers.PopBack();
				m_commandBufferThreads.insert({ cmdBuf, { threadId, secondary } });
				return cmdBuf;
			}
		}
		else
		{
			if (poolInfo.m_freePrimaryCommandBuffers.Count() > 0)
			{
				VkCommandBuffer cmdBuf = poolInfo.m_freePrimaryCommandBuffers.PopBack();
				m_commandBufferThreads.insert({ cmdBuf, { threadId, secondary } });
				return cmdBuf;
			}
		}

		// ...Otherwise allocate a new one.
		VkCommandBuffer newCmdBuf;
		VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
		allocInfo.commandBufferCount = 1;
		allocInfo.commandPool = poolInfo.m_pool;
		allocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		vkAllocateCommandBuffers(m_device.GetHandle(), &allocInfo, &newCmdBuf);
		PB_ASSERT(newCmdBuf);
		m_commandBufferThreads.insert({ newCmdBuf, { threadId, secondary } });

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

		//m_freeContextCmdBuffers[0].PushBack(context.GetCmdBuffer());
		FreeCommandBuffer(context.GetCmdBuffer());
		context.Invalidate();
	}

	CommandContext* Renderer::AllocateCommandContext()
	{
		if (m_freeCommandContexts.Count() > 0)
		{
			return m_freeCommandContexts.PopBack();
		}

		return m_commandContextAllocator.Alloc<CommandContext>();
	}

	void Renderer::FreeCommandContext(CommandContext* context)
	{
		context->Invalidate();
		m_freeCommandContexts.PushBack(context);
	}

	void Renderer::AddThreadCommandContext(ThreadCommandContext* context)
	{
		std::lock_guard<std::mutex> lock(m_threadContextLock);
		m_threadCommandContexts.PushBack(context);
	}

	void Renderer::EndFrame(float& outStallTimeMs)
	{
		{
			std::lock_guard<std::mutex> lock(m_threadContextLock);
			for (ThreadCommandContext*& context : m_threadCommandContexts)
			{
				context->End();
			}
			m_threadCommandContexts.Clear();
		}

		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];

		// Wait for frame to finish if it's still in-flight.
		std::chrono::time_point beforeWait = std::chrono::high_resolution_clock::now();
		vkWaitForFences(m_device.GetHandle(), 1, &curFrameInfo.m_frameFence, VK_TRUE, ~(0ULL));
		std::chrono::time_point afterWait = std::chrono::high_resolution_clock::now();
		vkResetFences(m_device.GetHandle(), 1, &curFrameInfo.m_frameFence);
		auto waitTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(afterWait - beforeWait).count();
		outStallTimeMs = float(double(waitTimeUs) / 1000.0f);

		// Reset frame...
		{
			//m_freeContextCmdBuffers[0] += curFrameInfo.m_enqueuedCmdBuffers; // Append submitted command buffers to free buffer array, as they are now no longer in-flight.
			for (auto& cmdBuf : curFrameInfo.m_enqueuedCmdBuffers)
			{
				FreeCommandBuffer(cmdBuf);
			}

			curFrameInfo.m_state = PB_FRAME_STATE_OPEN;

			m_uboDescSets += curFrameInfo.m_submittedUBODescSets;
			curFrameInfo.m_submittedUBODescSets.Clear();

			m_device.GetTempBufferAllocator().ResetFrame(m_curFrameInfoIdx);

			// Deferred deletions...
			for (DeferredDeletion*& deletion : curFrameInfo.m_deferredDeletions)
			{
				deletion->OnDelete(m_allocator);
			}
			curFrameInfo.m_deferredDeletions.Clear();
		}

		PB_ASSERT(curFrameInfo.m_frameSemaphore);
		if (m_validSwapchain)
		{
			VkResult res = vkAcquireNextImageKHR(m_device.GetHandle(), m_swapchain.GetHandle(), ~(0ULL), curFrameInfo.m_imageAcquireSempahore, VK_NULL_HANDLE, &curFrameInfo.m_presentImageIdx);
			PB_ASSERT(res == VK_SUCCESS);
		}

		SubmitFrame();

		if(m_validSwapchain)
			Present();

		if (m_validSwapchain && m_resetSwapchain)
		{
			WaitIdle();

			// Destroy sync objects.
			for (u32 i = 0; i < m_frameInfos.Count(); ++i)
			{
				vkDestroyFence(m_device.GetHandle(), m_frameInfos[i].m_frameFence, nullptr);
				vkDestroySemaphore(m_device.GetHandle(), m_frameInfos[i].m_imageAcquireSempahore, nullptr);
				vkDestroySemaphore(m_device.GetHandle(), m_frameInfos[i].m_frameSemaphore, nullptr);
			}

			m_swapchain.Destroy();
			CreateWindowSurface(&m_windowDesc);

			m_swapchain.Init(m_swapchainDesc, this, m_windowSurface);

			CreateSyncObjects();

			// TODO: This will invalidate ALL cached framebuffers, including those used not using resolution-scaled render targets. Maybe we should find a way to only destroy the framebuffers we need to.
			m_framebufferCache.Destroy();
			m_framebufferCache.Init(&m_device);

			m_resetSwapchain = false;
		}

		BeginNextFrame();
		++m_currentFrame;
	}

	void Renderer::WaitIdle()
	{
		PB_ERROR_CHECK(vkDeviceWaitIdle(m_device.GetHandle()));
		PB_BREAK_ON_ERROR;
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
		internalBuf->Release();
	}

	IResourcePool* Renderer::AllocateResourcePool(const ResourcePoolDesc& poolDesc)
	{
		auto* internalPool = m_allocator.Alloc<ResourcePool>();
		internalPool->Create(&m_device, poolDesc);
		return internalPool;
	}

	void Renderer::FreeResourcePool(IResourcePool* pool)
	{
		m_allocator.Free(reinterpret_cast<ResourcePool*>(pool));
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

	ResourceView Renderer::GetSampler(const SamplerDesc& samplerDesc)
	{
		return m_viewCache.GetSampler(samplerDesc);
	}

	IBindingCache* Renderer::AllocateBindingCache()
	{
		return m_allocator.Alloc<BindingCache>(&m_allocator);
	}

	void Renderer::FreeBindingCache(IBindingCache* cache)
	{
		m_allocator.Free(cache);
	}

	void Renderer::FreeCommandList(ICommandList* list)
	{
		std::lock_guard<std::mutex> lock(m_contextCmdReturnLock);

		CommandList* internalList = reinterpret_cast<CommandList*>(list);
		//m_freeContextCmdBuffers[1].PushBack(internalList->GetCommandBuffer());
		FreeCommandBuffer(internalList->GetCommandBuffer());
		for (auto& set : internalList->GetUBOSets())
			m_usedUBODescSets.PushBack(set);
		m_allocator.Free(internalList);
	}

	ICommandContext* Renderer::GetThreadUploadContext()
	{
		return t_threadResourceInitializationCommandContext.Get(this);
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
			return m_uboDescSets.PopBack();

		VkDescriptorSetAllocateInfo uboSetAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
		uboSetAllocInfo.descriptorPool = m_sharedDescPool;
		uboSetAllocInfo.descriptorSetCount = 1;
		uboSetAllocInfo.pSetLayouts = &m_uboSetLayout;
		uboSetAllocInfo.pNext = nullptr;
		
		VkDescriptorSet descSet = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkAllocateDescriptorSets(m_device.GetHandle(), &uboSetAllocInfo, &descSet));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(descSet);
		return descSet;
	}

	void Renderer::ReturnUBOSet(VkDescriptorSet set)
	{
		m_usedUBODescSets.PushBack(set);
	}

	VkDescriptorSetLayout Renderer::GetUBOSetLayout()
	{
		return m_uboSetLayout;
	}

	void Renderer::AddDeferredDeletion(DeferredDeletion* deletion)
	{
		std::lock_guard<std::mutex> deletionLock(m_deletionLock);
		m_pendingDeletions.PushBack(deletion);
	}

	CLib::Allocator& Renderer::GetAllocator()
	{
		return m_allocator;
	}

	u64 Renderer::GetCurrentFrame()
	{
		return m_currentFrame;
	}

	void Renderer::RegisterObjectName(const char* name, uint64_t objectHandle, VkObjectType objectType)
	{
#if PB_USE_DEBUG_UTILS
		VkDebugUtilsObjectNameInfoEXT nameInfo;
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.pNext = nullptr;
		nameInfo.objectHandle = objectHandle;
		nameInfo.objectType = objectType;
		nameInfo.pObjectName = name;

		auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(m_vkInstance.GetHandle(), "vkSetDebugUtilsObjectNameEXT");
		func(m_device.GetHandle(), &nameInfo);
#endif
	}

	void Renderer::CreateWindowSurface(WindowDesc* windowInfo)
	{
		if (m_windowSurface)
			vkDestroySurfaceKHR(m_vkInstance.GetHandle(), m_windowSurface, nullptr);

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
			vkCreateSemaphore(m_device.GetHandle(), &semaphoreInfo, nullptr, &frameInfo.m_imageAcquireSempahore);
			PB_ASSERT(frameInfo.m_imageAcquireSempahore);
			vkCreateSemaphore(m_device.GetHandle(), &semaphoreInfo, nullptr, &frameInfo.m_frameSemaphore);
			PB_ASSERT(frameInfo.m_frameSemaphore);
		}
	}

	void Renderer::CreatePoolAndSetLayouts()
	{
		CLib::Vector<VkDescriptorPoolSize, 1> poolSizes;

		VkDescriptorPoolSize& uboPoolSize = poolSizes.PushBack();
		uboPoolSize.descriptorCount = (m_device.GetDescriptorIndexingProperties()->maxPerStageDescriptorUpdateAfterBindUniformBuffers - 1) * MaxUBOSets;
		uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		VkDescriptorPoolCreateInfo sharedPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr };
		sharedPoolInfo.flags = 0;
		sharedPoolInfo.maxSets = 512 * PB_FRAME_IN_FLIGHT_COUNT;

		sharedPoolInfo.poolSizeCount = poolSizes.Count();
		sharedPoolInfo.pPoolSizes = poolSizes.Data();

		PB_ERROR_CHECK(vkCreateDescriptorPool(m_device.GetHandle(), &sharedPoolInfo, nullptr, &m_sharedDescPool));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_sharedDescPool);

		// TODO: Could we create layouts for each stage and pick one based upon where we're binding the set we allocate, rather than using a (potentially slower) one-fits-all layout?
		VkDescriptorSetLayoutBinding uboLayoutBinding;
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorCount = m_device.GetDescriptorIndexingProperties()->maxPerStageDescriptorUpdateAfterBindUniformBuffers - 1;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.pImmutableSamplers = nullptr;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

		// UBO sets may only be partially bound.
		VkDescriptorBindingFlags uboBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
		VkDescriptorSetLayoutBindingFlagsCreateInfo uboFlags{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr };
		uboFlags.bindingCount = 1;
		uboFlags.pBindingFlags = &uboBindingFlags;

		VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr };
		layoutInfo.flags = 0;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;
		layoutInfo.pNext = &uboFlags;

		// UBO set layout
		PB_ERROR_CHECK(vkCreateDescriptorSetLayout(m_device.GetHandle(), &layoutInfo, nullptr, &m_uboSetLayout));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_uboSetLayout);
	}

	void Renderer::BeginNextFrame()
	{
		if (m_curFrameInfoIdx + 1u < m_swapchain.GetImageCount() && m_curFrameInfoIdx + 1u < PB_FRAME_IN_FLIGHT_COUNT)
			++m_curFrameInfoIdx;
		else
			m_curFrameInfoIdx = 0;

		FrameInfo& curFrameInfo = m_frameInfos[m_curFrameInfoIdx];
		curFrameInfo.m_submittedInternalCmdBuffers.Clear();
		curFrameInfo.m_prioritySubmittedContextBuffers.Clear();
		curFrameInfo.m_submittedContextCmdBuffers.Clear();
		PB_ASSERT(curFrameInfo.m_state == PB_FRAME_STATE_IN_FLIGHT || curFrameInfo.m_state == PB_FRAME_STATE_OPEN);
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

		{
			std::lock_guard<std::mutex> deletionLock(m_deletionLock);

			curFrameInfo.m_deferredDeletions += m_pendingDeletions;
			m_pendingDeletions.Clear();
		}

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
		submitInfo.commandBufferCount = curFrameInfo.m_enqueuedCmdBuffers.Count();
		submitInfo.pCommandBuffers = curFrameInfo.m_enqueuedCmdBuffers.Data();
		submitInfo.signalSemaphoreCount = m_validSwapchain ? 1 : 0;
		submitInfo.pSignalSemaphores = &curFrameInfo.m_frameSemaphore;
		submitInfo.waitSemaphoreCount = m_validSwapchain ? 1 : 0; // No need to wait on acquire when there is no Swapchain to acquire from.
		submitInfo.pWaitSemaphores = &curFrameInfo.m_imageAcquireSempahore;

		// Wait for color attachment output before signalling the frame semaphore.
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.pWaitDstStageMask = waitStages;

		// Signal the frame fence when frame commands have finished execution.
		VkResult error = VK_SUCCESS;
		PB_ERROR_CHECK(error = vkQueueSubmit(m_presentQueue, 1, &submitInfo, curFrameInfo.m_frameFence));
		PB_BREAK_ON_ERROR;
	}

	void Renderer::FreeCommandBuffer(VkCommandBuffer cmdBuf)
	{
		auto it = m_commandBufferThreads.find(cmdBuf);
		PB_ASSERT(it != m_commandBufferThreads.end());

		auto [threadId, secondary] = it->second;
		
		auto poolIt = m_liveCommandPools.find(threadId);
		PB_ASSERT(poolIt != m_liveCommandPools.end());
		CommandPoolInfo& poolInfo = *poolIt->second;
		if (secondary)
		{
			poolInfo.m_freeSecondaryCommandBuffers.PushBack(cmdBuf);
		}
		else
		{
			poolInfo.m_freePrimaryCommandBuffers.PushBack(cmdBuf);
		}

		if (--poolInfo.m_allocatedCmdBufferCount == 0)
		{
			m_freeCommandPools.PushBack(&poolInfo);
			m_liveCommandPools.erase(poolIt);
		}
		m_commandBufferThreads.erase(it);
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

		VkResult res = vkQueuePresentKHR(m_presentQueue, &presentInfo);
		PB_ASSERT(res == VK_SUCCESS || res == VK_ERROR_OUT_OF_DATE_KHR);

		curFrameInfo.m_state = PB_FRAME_STATE_IN_FLIGHT;
	}
}
