#include "PoolAllocator.h"
#include "Renderer.h"

// May help in diagnosing issues with suballocation.
#define POOL_ALLOCATOR_DISABLE_SUBALLOCATION false

namespace PB
{
	class MemoryDeferredDeletion : public DeferredDeletion
	{
	public:

		MemoryDeferredDeletion(VkDevice device, VkDeviceMemory memory)
		{
			m_device = device;
			m_memory = memory;
		}
		~MemoryDeferredDeletion() = default;

		void OnDelete(CLib::Allocator& allocator) override
		{
			vkFreeMemory(m_device, m_memory, nullptr);
			m_memory = VK_NULL_HANDLE;

			allocator.Free(this);
		}

	private:

		VkDevice m_device;
		VkDeviceMemory m_memory;
	};

	void PoolAllocator::Init(Renderer* renderer, EMemoryType memoryType, uint32_t poolSize, uint32_t minAlignmentBytes, uint64_t poolMemoryLimit, const CLib::Vector<uint32_t>& segments)
	{
		m_renderer = renderer;

		Device* device = renderer->GetDevice();

		m_device = device->GetHandle();
		m_memoryTypeIndex = device->FindMemoryTypeIndex(~u32(0), memoryType);
		m_memoryType = memoryType;
		m_minAlignment = minAlignmentBytes;
		m_poolSize = poolSize;
		m_totalAllocatedMemory = m_poolSize;
		m_poolMemoryLimit = poolMemoryLimit == 0 ? ~uint64_t(0) : poolMemoryLimit;
		m_poolSuballocator.Init(segments.Data(), segments.Count(), this, PoolAlloc, PoolFree);
	}

	PoolAllocator::~PoolAllocator()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		// Memory pools should be cleaned up when all allocations belonging to them are freed.
		// Destruction of m_poolSuballocator should clean up memory pools which persist.
	}

	void PoolAllocator::Alloc(uint32_t sizeBytes, uint32_t alignBytes, PoolAllocation& outAllocation)
	{
		if (m_memoryType == PB::EMemoryType::HOST_VISIBLE || m_memoryType == PB::EMemoryType::HOST_VISIBLE_AND_DEVICE_LOCAL)
		{
			// Round up size to a multiple of VkPhysicalDeviceLimits::nonCoherentAtomSize - as required by the Vulkan spec for flushing mapped memory.
			// This should still flush the desired range of memory, just with a little excess.
			uint32_t sizeMultiple = uint32_t(m_renderer->GetDevice()->GetDeviceLimits()->nonCoherentAtomSize);
			sizeBytes += (sizeMultiple - (sizeBytes % sizeMultiple));
		}

		if constexpr (POOL_ALLOCATOR_DISABLE_SUBALLOCATION)
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			outAllocation.m_ptr = nullptr;
			outAllocation.m_offset = 0;
			outAllocation.m_size = sizeBytes;

			VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
			allocInfo.allocationSize = sizeBytes;
			allocInfo.memoryTypeIndex = m_memoryTypeIndex;

			PB_ERROR_CHECK(vkAllocateMemory(m_device, &allocInfo, nullptr, &outAllocation.m_memoryHandle));
			PB_BREAK_ON_ERROR;

			if (m_memoryType == PB::EMemoryType::HOST_VISIBLE || m_memoryType == PB::EMemoryType::HOST_VISIBLE_AND_DEVICE_LOCAL)
			{
				PB_ERROR_CHECK(vkMapMemory(m_device, outAllocation.m_memoryHandle, 0, sizeBytes, 0, &outAllocation.m_ptr));
				PB_BREAK_ON_ERROR;
			}
			return;
		}

		if (alignBytes < m_minAlignment)
		{
			alignBytes = m_minAlignment;
		}

		std::lock_guard<std::mutex> lock(m_mutex);

		void* address = m_poolSuballocator.Alloc(sizeBytes, alignBytes);
		void* pageAddress = m_poolSuballocator.GetAllocationPageAddress(address);

		auto pageIt = m_pools.find(pageAddress);
		PB_ASSERT(pageIt != m_pools.end());

		outAllocation.m_ptr = pageIt->first;
		outAllocation.m_memoryHandle = pageIt->second.m_memory;
		outAllocation.m_offset = static_cast<uint32_t>(reinterpret_cast<size_t>(address) - reinterpret_cast<size_t>(outAllocation.m_ptr));
		outAllocation.m_size = sizeBytes;
	}

	void PoolAllocator::Free(PoolAllocation& allocation)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if constexpr (POOL_ALLOCATOR_DISABLE_SUBALLOCATION)
		{
			if (m_memoryType == PB::EMemoryType::HOST_VISIBLE || m_memoryType == PB::EMemoryType::HOST_VISIBLE_AND_DEVICE_LOCAL)
			{
				vkUnmapMemory(m_device, allocation.m_memoryHandle);
			}
			m_renderer->AddDeferredDeletion(m_renderer->GetAllocator().Alloc<MemoryDeferredDeletion>(m_device, allocation.m_memoryHandle));

			allocation.m_ptr = nullptr;
			allocation.m_memoryHandle = VK_NULL_HANDLE;
			return;
		}

		void* address = reinterpret_cast<void*>(reinterpret_cast<size_t>(allocation.m_ptr) + allocation.m_offset);
		m_poolSuballocator.Free(address);
		allocation.m_ptr = nullptr;
		allocation.m_memoryHandle = VK_NULL_HANDLE;
	}

	void* PoolAllocator::PoolAlloc(void* context, uint32_t requestedMinSize, uint32_t& outSize)
	{
		PoolAllocator* self = reinterpret_cast<PoolAllocator*>(context);

		uint32_t pageSize = requestedMinSize > self->m_poolSize ? requestedMinSize : self->m_poolSize;

		// Device memory can be used as the base ptr, as it is going to be a valid location in Host/Device-side memory.
		VkDeviceMemory memory = VK_NULL_HANDLE;

		VkMemoryAllocateFlagsInfo allocFlags{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr };
		allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &allocFlags };
		allocInfo.allocationSize = pageSize;
		allocInfo.memoryTypeIndex = self->m_memoryTypeIndex;

		PB_ERROR_CHECK(vkAllocateMemory(self->m_device, &allocInfo, nullptr, &memory));
		PB_BREAK_ON_ERROR;
		std::pair<void*, PoolInfo> newPair{ reinterpret_cast<void*>(self->m_memoryHead), { memory, pageSize } };
		if (self->m_memoryType == EMemoryType::HOST_VISIBLE || self->m_memoryType == EMemoryType::HOST_VISIBLE_AND_DEVICE_LOCAL)
		{
			PB_ERROR_CHECK(vkMapMemory(self->m_device, memory, 0, pageSize, 0, &newPair.first));
			PB_BREAK_ON_ERROR;
		}
		self->m_memoryHead += pageSize;
		self->m_totalAllocatedMemory += pageSize;
		assert(self->m_totalAllocatedMemory <= self->m_poolMemoryLimit && "PoolAllocator has exceeded its memory limit.");

		outSize = uint32_t(allocInfo.allocationSize);
		self->m_pools.insert(newPair);
		return newPair.first;
	}

	void PoolAllocator::PoolFree(void* context, void* ptr)
	{
		PoolAllocator* self = reinterpret_cast<PoolAllocator*>(context);

		auto it = self->m_pools.find(ptr);
		PB_ASSERT(it != self->m_pools.end());

		if (self->m_memoryType == EMemoryType::HOST_VISIBLE || self->m_memoryType == EMemoryType::HOST_VISIBLE_AND_DEVICE_LOCAL)
		{
			vkUnmapMemory(self->m_device, it->second.m_memory);
		}
		
		// Schedule deferred deletion of memory.
		{
			Renderer* renderer = self->m_renderer;
			CLib::Allocator& allocator = renderer->GetAllocator();

			renderer->AddDeferredDeletion(allocator.Alloc<MemoryDeferredDeletion>(self->m_device, it->second.m_memory));
		}

		self->m_totalAllocatedMemory -= it->second.m_poolSize;
		self->m_pools.erase(it);
	}
}