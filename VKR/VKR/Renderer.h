#pragma once
#include "IRenderer.h"
#include "ParaBlitApi.h"
#include "VulkanInstance.h"
#include "Device.h"
#include "Swapchain.h"
#include "CLib/Vector.h"
#include "CLib/Allocator.h"
#include "CmdContextPool.h"
#include "RenderPassCache.h"
#include "ImageView.h"
#include "FramebufferCache.h"
#include "ShaderModule.h"
#include "PipelineCache.h"
#include "BufferObject.h"

#include <mutex>

namespace PB 
{
	enum EFrameState
	{
		PB_FRAME_STATE_OPEN,
		PB_FRAME_STATE_RECORDING,
		PB_FRAME_STATE_IN_FLIGHT
	};

	// A Dynamic Resource Index (DRI) buffer is a dynamic uniform buffer that contains indices used for descriptor indexing for an entire frame. 
	// A shader will view part of this buffer using dynamic offsets to access the indices for its resources.
	struct DRIBuffer
	{
		BufferObject m_buffer;
		BufferObject m_stagingBuffer;
		VkDescriptorSet m_descSet = VK_NULL_HANDLE;
		u32 m_currentOffset = 0;
		bool m_isFull = false;
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
		CLib::Vector<VkCommandBuffer, 8> m_submittedContextCmdBuffers;		// Command context command buffers submitted for this frame, these will be emptied when the frame is next ready for use.
		CLib::Vector<VkCommandBuffer, 8> m_prioritySubmittedContextBuffers;	// Submitted context command buffers that will be executed before non-priority command buffers.
		CLib::Vector<VkCommandBuffer, 8> m_submittedInternalCmdBuffers;		// Submitted context command buffers that will be executed before non-priority command buffers.
		CLib::Vector<VkCommandBuffer, 24> m_enqueuedCmdBuffers;				// Contains all command buffers which have been submitted to the queue.
		CLib::Vector<BufferObject, 8> m_stagingBuffers;						// Staging buffers which are in flight for this frame. These will be deleted or re-used once the frame is complete.
		CLib::Vector<DRIBuffer> m_driBuffers;								// Contains dynamic resource indices (DRI) for indexing descriptors in shaders. Each buffer also has an associated descriptor set for binding it.
	};

	class Renderer : public IRenderer
	{
	public:

		static constexpr const u32 DRIBufferSize = UINT16_MAX; // TODO: Ensure this is less than or equal to the uniform buffer size device limit.
		static constexpr const u32 MaxDRIBufferCountPerFrame = 3;

		PARABLIT_API Renderer();

		PARABLIT_API ~Renderer();

		PARABLIT_API void Init(const RendererDesc& desc) override;

		PARABLIT_API ISwapChain* CreateSwapChain(const SwapChainDesc& desc) override;

		PARABLIT_API Device* GetDevice();

		PARABLIT_API IRenderPassCache* GetRenderPassCache() override;

		PARABLIT_API ViewCache* GetViewCache();

		PARABLIT_API IShaderModuleCache* GetShaderModuleCache() override;

		PARABLIT_API IPipelineCache* GetPipelineCache() override;

		PARABLIT_API FramebufferCache* GetFramebufferCache();

		PARABLIT_API VkCommandBuffer AllocateCommandBuffer();

		PARABLIT_API void ReturnCommandBuffer(CommandContext& context);

		PARABLIT_API void ReturnOpenBuffer(CommandContext& context);

		PARABLIT_API void EndFrame() override;

		PARABLIT_API void WaitIdle() override;

		PARABLIT_API u32 GetCurrentSwapchainImageIndex() override;

		PARABLIT_API IBufferObject* AllocateBuffer(const BufferObjectDesc& bufDesc) override;

		PARABLIT_API void FreeBuffer(IBufferObject* buffer) override;

		PARABLIT_API ITexture* AllocateTexture(const TextureDesc& texDesc) override;

		PARABLIT_API void FreeTexture(ITexture* texture) override;

		Sampler GetSampler(const SamplerDesc& samplerDesc);

		PARABLIT_API CmdContextPool& GetContextPool();

		DRIBuffer* GetDRIBuffer();

		VkDescriptorSetLayout GetDRISetLayout();

		u64 GetCurrentFrame();

	private:

		inline void CreateWindowSurface(WindowDesc* windowHandle) override;

		inline void CreateSyncObjects();

		inline void CreateCmdBuffers();

		inline void CreateDRIPoolAndSetLayout();

		inline void CreateDRIBuffer(DRIBuffer& buffer);

		// Reset frame tracking to begin recording the next frame.
		inline void BeginNextFrame();

		inline void InlineContextCmdBuffers();

		inline void SubmitFrame();

		inline void Present();

		VulkanInstance m_vkInstance;
		Device m_device;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		SwapChainDesc m_swapchainDesc;
		Swapchain m_swapchain;

		// Misc
		CLib::Allocator m_allocator;

		// Resource Cache
		RenderPassCache m_renderPassCache;
		ViewCache m_viewCache;
		FramebufferCache m_framebufferCache;
		ShaderCache m_shaderModuleCache;
		PipelineCache m_pipelineCache;

		// Frame State
		VkQueue m_presentQueue = VK_NULL_HANDLE;
		VkCommandPool m_masterCmdPool = VK_NULL_HANDLE;
		VkDescriptorPool m_driBufferDescPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_driSetLayout = VK_NULL_HANDLE;
		u64 m_currentFrame = 0;
		u8 m_curFrameInfoIdx = 0;
		u8 m_lastFrameInfoIdx = ~0;
		CLib::Vector<FrameInfo, PB_FRAME_IN_FLIGHT_COUNT> m_frameInfos;
		CLib::Vector<VkCommandBuffer, PB_FRAME_IN_FLIGHT_COUNT> m_masterCmdBuffers;
		CLib::Vector<VkCommandBuffer, 64> m_freeContextCmdBuffers;
		CLib::Vector<VkCommandBuffer, 32> m_internalCmdBuffers;
		std::mutex m_contextCmdAllocLock;
		std::mutex m_contextCmdReturnLock;
		CmdContextPool m_contextPool;
	};
}

