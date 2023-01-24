#pragma once
#include "IRenderer.h"
#include "ParaBlitApi.h"
#include "VulkanInstance.h"
#include "Device.h"
#include "Swapchain.h"
#include "CLib/Vector.h"
#include "CLib/Allocator.h"
#include "CLib/FixedBlockAllocator.h"
#include "CommandContext.h"
#include "RenderPassCache.h"
#include "ImageView.h"
#include "FramebufferCache.h"
#include "ShaderModule.h"
#include "PipelineCache.h"
#include "BufferObject.h"
#include "ResourcePool.h"

#include <mutex>

namespace PB 
{
	enum EFrameState
	{
		PB_FRAME_STATE_OPEN,
		PB_FRAME_STATE_RECORDING,
		PB_FRAME_STATE_IN_FLIGHT
	};

	// Wrapper class for deleting objects (meshes/textures) which will be deleted after the last from using it is reset/retired.
	class DeferredDeletion
	{
	public:

		virtual void OnDelete(CLib::Allocator& allocator) = 0;
	};

	struct FrameInfo // Stores the properties and resources unique to a single frame.
	{
		VkImage m_presentImage = VK_NULL_HANDLE;
		VkSemaphore m_imageAcquireSempahore = VK_NULL_HANDLE;				// Signalled when the swap chain image has been acquired, rendering will wait on this.
		VkSemaphore m_frameSemaphore = VK_NULL_HANDLE;						// Signalled when the frame is complete. This semaphore will be waited on before the image is presented.
		VkFence m_frameFence = VK_NULL_HANDLE;								// This semaphore indicates if this frame is currently in-flight.
		VkCommandBuffer m_masterCommandBuffer = VK_NULL_HANDLE;				// Contains all recorded graphics commands for the frame.
		EFrameState m_state = PB_FRAME_STATE_OPEN;							// TODO: Evaluate if this state enum is even needed outside of validation.
		u32 m_presentImageIdx = 0;											// Swapchain image index.
		CLib::Vector<VkCommandBuffer, 8> m_submittedContextCmdBuffers;		// Command context command buffers submitted for this frame, these will be emptied when the frame is next ready for use.
		CLib::Vector<VkCommandBuffer, 8> m_prioritySubmittedContextBuffers;	// Submitted context command buffers that will be executed before non-priority command buffers.
		CLib::Vector<VkCommandBuffer, 8> m_submittedInternalCmdBuffers;		// Submitted context command buffers that will be executed before non-priority command buffers.
		CLib::Vector<VkCommandBuffer, 24> m_enqueuedCmdBuffers;				// Contains all command buffers which have been submitted to the queue.
		CLib::Vector<VkDescriptorSet, 16, 16> m_submittedUBODescSets;		// Contains this frame's in-flight UBO descriptor sets.
		CLib::Vector<DeferredDeletion*, 16, 16> m_deferredDeletions;		// Contains references to resources to be deleted when the frame is complete.
	};

	class Renderer : public IRenderer
	{
	public:

		static thread_local ThreadCommandContext t_threadResourceInitializationCommandContext;

		PARABLIT_API Renderer();

		PARABLIT_API ~Renderer();

		PARABLIT_API void Init(const RendererDesc& desc) override;

		PARABLIT_API ISwapChain* CreateSwapChain(const SwapChainDesc& desc) override;

		PARABLIT_API void RecreateSwapchain(const SwapChainDesc& desc, WindowDesc* windowDesc) override;

		PARABLIT_API Device* GetDevice();

		PARABLIT_API IRenderPassCache* GetRenderPassCache() override;

		PARABLIT_API ViewCache* GetViewCache();

		ISwapChain* GetSwapchain() override;

		bool HasValidSwapchain() override;

		PARABLIT_API IShaderModuleCache* GetShaderModuleCache() override;

		PARABLIT_API IPipelineCache* GetPipelineCache() override;

		PARABLIT_API IFramebufferCache* GetFramebufferCache() override;

		PARABLIT_API VkCommandBuffer AllocateCommandBuffer(bool secondary);

		PARABLIT_API void ReturnCommandBuffer(CommandContext& context);

		PARABLIT_API void ReturnOpenBuffer(CommandContext& context);

		PARABLIT_API CommandContext* AllocateCommandContext();

		PARABLIT_API void FreeCommandContext(CommandContext* context);

		PARABLIT_API void EndFrame(float& outStallTimeMs) override;

		PARABLIT_API void WaitIdle() override;

		PARABLIT_API u32 GetCurrentSwapchainImageIndex() override;

		PARABLIT_API IBufferObject* AllocateBuffer(const BufferObjectDesc& bufDesc) override;

		PARABLIT_API void FreeBuffer(IBufferObject* buffer) override;

		IResourcePool* AllocateResourcePool(const ResourcePoolDesc& poolDesc) override;

		void FreeResourcePool(IResourcePool* pool) override;

		PARABLIT_API ITexture* AllocateTexture(const TextureDesc& texDesc) override;

		PARABLIT_API void FreeTexture(ITexture* texture) override;

		ResourceView GetSampler(const SamplerDesc& samplerDesc);

		IBindingCache* AllocateBindingCache() override;

		void FreeBindingCache(IBindingCache* cache) override;

		void FreeCommandList(ICommandList* list) override;

		VkDescriptorSet GetMasterSet();

		VkDescriptorSetLayout GetMasterSetLayout();

		VkDescriptorSet GetUBOSet();

		void ReturnUBOSet(VkDescriptorSet set);

		VkDescriptorSetLayout GetUBOSetLayout();

		void AddDeferredDeletion(DeferredDeletion* deletion);

		CLib::Allocator& GetAllocator();

		u64 GetCurrentFrame();

		void RegisterObjectName(const char* name, uint64_t objectHandle, VkObjectType objectType);

	private:

		inline void CreateWindowSurface(WindowDesc* windowHandle) override;

		inline void CreateSyncObjects();

		inline void CreateCmdBuffers();

		inline void CreatePoolAndSetLayouts();

		// Reset frame tracking to begin recording the next frame.
		inline void BeginNextFrame();

		inline void SubmitFrame();

		inline void Present();

		VulkanInstance m_vkInstance;
		Device m_device;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		WindowDesc m_windowDesc{};
		SwapChainDesc m_swapchainDesc;
		Swapchain m_swapchain;
		bool m_validSwapchain = false;

		// Misc
		CLib::Allocator m_allocator;

		// Resource Cache
		RenderPassCache m_renderPassCache;
		ViewCache m_viewCache;
		FramebufferCache m_framebufferCache;
		ShaderCache m_shaderModuleCache;
		PipelineCache m_pipelineCache;
		VkDescriptorSet m_masterResourceDescSet = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_masterSetLayout = VK_NULL_HANDLE;

		// Shared descriptors
		static constexpr const u32 MaxUBOSets = 512;

		VkCommandPool m_masterCmdPool = VK_NULL_HANDLE;
		VkDescriptorPool m_sharedDescPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_uboSetLayout = VK_NULL_HANDLE;

		// Frame State
		VkQueue m_presentQueue = VK_NULL_HANDLE;
		u64 m_currentFrame = 0;
		u8 m_curFrameInfoIdx = 0;
		bool m_resetSwapchain = false;
		CLib::Vector<FrameInfo, PB_FRAME_IN_FLIGHT_COUNT> m_frameInfos;
		CLib::Vector<VkDescriptorSet, 16> m_uboDescSets;
		CLib::Vector<VkDescriptorSet, 16> m_usedUBODescSets;
		CLib::Vector<VkCommandBuffer, PB_FRAME_IN_FLIGHT_COUNT> m_masterCmdBuffers;
		CLib::Vector<VkCommandBuffer, 32> m_freeContextCmdBuffers[2];
		CLib::Vector<VkCommandBuffer, 32> m_internalCmdBuffers;
		CLib::Vector<DeferredDeletion*, 16, 16> m_pendingDeletions;
		std::mutex m_contextCmdAllocLock;
		std::mutex m_contextCmdReturnLock;
		std::mutex m_deletionLock;

		CLib::FixedBlockAllocator m_commandContextAllocator{ sizeof(CommandContext), sizeof(CommandContext) * 16 };
		CLib::Vector<CommandContext*, 8, 8> m_freeCommandContexts;
	};
}

