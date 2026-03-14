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
#include "ImGUIModule/ImGUIModule.h"
#include "vulkan/vulkan_core.h"

#include <mutex>
#include <thread>

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
		VkSemaphore m_imageAcquireSempahore = VK_NULL_HANDLE;															// Signalled when the swap chain image has been acquired, rendering will wait on this.
		VkFence m_frameFence = VK_NULL_HANDLE;																			// This semaphore indicates if this frame is currently in-flight.
		EFrameState m_state = PB_FRAME_STATE_OPEN;																		// TODO: Evaluate if this state enum is even needed outside of validation.
		u32 m_presentImageIdx = 0;																						// Swapchain image index. Also used to select the correct semaphore for frame execution and image presentation.
		CLib::Vector<VkCommandBuffer, 8> m_submittedContextCmdBuffers;													// Command context command buffers submitted for this frame, these will be emptied when the frame is next ready for use.
		CLib::Vector<VkCommandBuffer, 8> m_prioritySubmittedContextBuffers;												// Submitted context command buffers that will be executed before non-priority command buffers.
		CLib::Vector<VkCommandBuffer, 8> m_submittedInternalCmdBuffers;													// Submitted context command buffers that will be executed before non-priority command buffers.
		CLib::Vector<VkCommandBuffer, 24> m_enqueuedCmdBuffers;															// Contains all command buffers which have been submitted to the queue.
		CLib::Vector<VkDescriptorSet, 16, 16> m_submittedDescSets[u32(EDescriptorSetType::DESCRIPTOR_TYPE_COUNT)];	// Contains this frame's in-flight descriptor sets.
		CLib::Vector<DeferredDeletion*, 16, 16> m_deferredDeletions;													// Contains references to resources to be deleted when the frame is complete.
	};

	class Renderer : public IRenderer
	{
	public:

		static thread_local ThreadCommandContext t_threadResourceInitializationCommandContext;

		static constexpr const u32 MasterDescriptorSetIndex = 0;
		static constexpr const u32 UBODescriptorSetIndex = 1;
		static constexpr const u32 AccelerationStructureDescriptorSetIndex = 2;

		PARABLIT_API Renderer();

		PARABLIT_API ~Renderer();

		PARABLIT_API void Init(const RendererDesc& desc) override;

		PARABLIT_API ISwapChain* CreateSwapChain(const SwapChainDesc& desc) override;

		PARABLIT_API void RecreateSwapchain(const SwapChainDesc& desc, WindowDesc* windowDesc) override;

		PARABLIT_API Device* GetDevice();

		PARABLIT_API IRenderPassCache* GetRenderPassCache() override;

		PARABLIT_API ViewCache* GetViewCache();

		ISwapChain* GetSwapchain() override;

		const DeviceLimitations* GetDeviceLimitations() const override;

		bool HasValidSwapchain() override;

		PARABLIT_API IShaderModuleCache* GetShaderModuleCache() override;

		PARABLIT_API IPipelineCache* GetPipelineCache() override;

		PARABLIT_API IFramebufferCache* GetFramebufferCache() override;

		PARABLIT_API VkCommandBuffer AllocateCommandBuffer(bool secondary);

		PARABLIT_API void ReturnCommandBuffer(CommandContext& context);

		PARABLIT_API void ReturnOpenBuffer(CommandContext& context);

		PARABLIT_API CommandContext* AllocateCommandContext();

		PARABLIT_API void FreeCommandContext(CommandContext* context);

		void AddThreadCommandContext(ThreadCommandContext* context);

		PARABLIT_API void EndFrame(double& outStallTimeMs) override;

		PARABLIT_API void WaitIdle() override;

		PARABLIT_API u32 GetCurrentSwapchainImageIndex() override;

		PARABLIT_API IBufferObject* AllocateBuffer(const BufferObjectDesc& bufDesc) override;

		PARABLIT_API void FreeBuffer(IBufferObject* buffer) override;

		IResourcePool* AllocateResourcePool(const ResourcePoolDesc& poolDesc) override;

		void FreeResourcePool(IResourcePool* pool) override;

		IAccelerationStructure* AllocateAccelerationStructure(const AccelerationStructureDesc& desc) override;

		void FreeAccelerationStructure(IAccelerationStructure* as) override;

		PARABLIT_API ITexture* AllocateTexture(const TextureDesc& texDesc) override;

		PARABLIT_API void FreeTexture(ITexture* texture) override;

		ResourceView GetSampler(const SamplerDesc& samplerDesc);

		IBindingCache* AllocateBindingCache() override;

		void FreeBindingCache(IBindingCache* cache) override;

		void FreeCommandList(ICommandList* list) override;

		ICommandContext* GetThreadUploadContext() override;
		void ReturnThreadUploadContext(ICommandContext* context) override;

		IImGUIModule* InitImGUIModule(ImGuiContext* context) override;
		IImGUIModule* GetImGUIModule() override;

		VkDescriptorSet GetMasterSet();

		VkDescriptorSetLayout GetMasterSetLayout();

		VkDescriptorSet GetDescriptorSet(EDescriptorSetType descType);

		void ReturnDescriptorSet(VkDescriptorSet set, EDescriptorSetType descType);

		VkDescriptorSetLayout GetUBOSetLayout() { return m_uboSetLayout; }

		VkDescriptorSetLayout GetAccelerationStructureSetLayout() { return m_asSetLayout; }

		void AddDeferredDeletion(DeferredDeletion* deletion);

		CLib::Allocator& GetAllocator();

		u64 GetCurrentFrame();

		void RegisterObjectName(const char* name, uint64_t objectHandle, VkObjectType objectType);

		VulkanInstance& GetInstance() { return m_vkInstance; }

		VkQueue GetPresentQueue() { return m_presentQueue; }

	private:

		struct CommandPoolInfo
		{
			VkCommandPool m_pool = VK_NULL_HANDLE;
			CLib::Vector<VkCommandBuffer, 4, 4> m_freePrimaryCommandBuffers;
			CLib::Vector<VkCommandBuffer, 4, 4> m_freeSecondaryCommandBuffers;
			uint32_t m_allocatedCmdBufferCount = 0;
		};

		void CreateWindowSurface(WindowDesc* windowHandle) override;

		void CreateSyncObjects();

		void CreateUBODescriptorPool();

		void CreateSetLayouts();

		// Reset frame tracking to begin recording the next frame.
		void BeginNextFrame();

		void SubmitFrame();

		void FreeCommandBuffer(VkCommandBuffer cmdBuf);

		void Present();

		VulkanInstance m_vkInstance;
		Device m_device;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		WindowDesc m_windowDesc{};
		SwapChainDesc m_swapchainDesc;
		Swapchain m_swapchain;
		ImGUIModule* m_imguiModule = nullptr;
		bool m_validSwapchain = false;

		// Misc
		CLib::Allocator m_allocator{ 1024 * 1024, true };

		// Resource Cache
		RenderPassCache m_renderPassCache;
		ViewCache m_viewCache;
		FramebufferCache m_framebufferCache;
		ShaderCache m_shaderModuleCache;
		PipelineCache m_pipelineCache;
		VkDescriptorSet m_masterResourceDescSet = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_masterSetLayout = VK_NULL_HANDLE;

		// Shared descriptors
		static constexpr const u32 MaxUBOSetsPerPool = 128;
		static constexpr const u32 MaxUBOBindings = 16;
		static constexpr const u32 MaxAccelerationStructureBindings = 8;

		VkDescriptorSetLayout m_uboSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_asSetLayout = VK_NULL_HANDLE;

		// Frame State
		VkQueue m_presentQueue = VK_NULL_HANDLE;
		u64 m_currentFrame = 0;
		u8 m_curFrameInfoIdx = 0;
		bool m_resetSwapchain = false;
		CLib::Vector<FrameInfo, PB_FRAME_IN_FLIGHT_COUNT + 1, 1, true> m_frameInfos;
		CLib::Vector<VkSemaphore, PB_FRAME_IN_FLIGHT_COUNT + 1> m_frameSemaphores;
		CLib::Vector<VkDescriptorPool, 4, 4> m_uboDescriptorPools;
		CLib::Vector<VkDescriptorSet, 16> m_descriptorSets[u32(EDescriptorSetType::DESCRIPTOR_TYPE_COUNT)];
		CLib::Vector<VkDescriptorSet, 16> m_usedDescriptorSets[u32(EDescriptorSetType::DESCRIPTOR_TYPE_COUNT)];
		CLib::Vector<CommandPoolInfo*, 4, 4> m_freeCommandPools;
		// Each unique thread using a command buffer must have its own pool. Vulkan does not allow command buffers belonging to the same pool to be used on multiple threads.
		std::unordered_map<std::thread::id, CommandPoolInfo*> m_liveCommandPools;
		std::unordered_map<VkCommandBuffer, std::pair<std::thread::id, bool>> m_commandBufferThreads;
		CLib::Vector<VkCommandBuffer, 16> m_internalCmdBuffers;
		CLib::Vector<DeferredDeletion*, 16, 16> m_pendingDeletions;
		std::mutex m_contextCmdAllocLock;
		std::mutex m_contextCmdReturnLock;
		std::mutex m_threadContextLock;
		std::mutex m_deletionLock;

		CLib::FixedBlockAllocator m_commandContextAllocator{ sizeof(CommandContext), sizeof(CommandContext) * 16 };
		CLib::Vector<ThreadCommandContext*, 8, 8> m_threadCommandContexts;
		CLib::Vector<CommandContext*, 8, 8> m_freeCommandContexts;
	};
}

