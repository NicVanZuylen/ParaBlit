#include "PoolAllocator.h"
#include "Renderer.h"

namespace PB
{
	void PoolAllocator::Init(Device* device, EMemoryType memoryType, uint32_t poolSize, uint32_t minAlignmentBytes, const CLib::Vector<uint32_t>& segments)
	{
		m_device = device->GetHandle();
		m_memoryTypeIndex = device->FindMemoryTypeIndex(~u32(0), memoryType);
		m_memoryType = memoryType;
		m_minAlignment = minAlignmentBytes;
		m_poolSize = poolSize;
		m_totalAllocatedMemory = m_poolSize;
		m_poolSuballocator.Init(segments.Data(), segments.Count(), this, PoolAlloc, PoolFree);
	}

	PoolAllocator::~PoolAllocator()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		// Destruction of m_poolSuballocator should clean up memory pools.
	}

	void PoolAllocator::Alloc(uint32_t sizeBytes, uint32_t alignBytes, PoolAllocation& outAllocation)
	{
		if (alignBytes < m_minAlignment)
		{
			if (alignBytes == 0)
				alignBytes = m_minAlignment;
			else
			{
				uint32_t alignPad = m_minAlignment % alignBytes;
				alignBytes = m_minAlignment + alignBytes;
			}
		}

		std::lock_guard<std::mutex> lock(m_mutex);

		void* address = m_poolSuballocator.Alloc(sizeBytes, alignBytes);

		auto it = std::prev(m_pools.upper_bound(address));
		PB_ASSERT(it != m_pools.end());

		outAllocation.m_ptr = it->first;
		outAllocation.m_memoryHandle = it->second;
		outAllocation.m_offset = static_cast<uint32_t>(reinterpret_cast<size_t>(address) - reinterpret_cast<size_t>(outAllocation.m_ptr));
		outAllocation.m_size = sizeBytes;
	}

	void PoolAllocator::Free(PoolAllocation& allocation)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

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
		VkMemoryAllocateInfo allocInfo;
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.allocationSize = pageSize;
		allocInfo.memoryTypeIndex = self->m_memoryTypeIndex;

		PB_ERROR_CHECK(vkAllocateMemory(self->m_device, &allocInfo, nullptr, &memory));
		PB_BREAK_ON_ERROR;

		std::pair<void*, VkDeviceMemory> newPair{ reinterpret_cast<void*>(self->m_totalAllocatedMemory), memory };
		if (self->m_memoryType == EMemoryType::HOST_VISIBLE)
		{
			PB_ERROR_CHECK(vkMapMemory(self->m_device, memory, 0, pageSize, 0, &newPair.first));
			PB_BREAK_ON_ERROR;
		}
		self->m_totalAllocatedMemory += pageSize;

		outSize = uint32_t(allocInfo.allocationSize);
		self->m_pools.insert(newPair);
		return newPair.first;
	}

	void PoolAllocator::PoolFree(void* context, void* ptr)
	{
		PoolAllocator* self = reinterpret_cast<PoolAllocator*>(context);

		auto it = self->m_pools.find(ptr);
		PB_ASSERT(it != self->m_pools.end());

		if (self->m_memoryType == EMemoryType::HOST_VISIBLE)
		{
			vkUnmapMemory(self->m_device, it->second);
		}
		vkFreeMemory(self->m_device, it->second, nullptr);
		self->m_pools.erase(it);
	}
}