#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"

#include "CLib/ExternalAllocator.h"

#include <map>
#include <mutex>

namespace PB
{
	class Renderer;

	class PoolAllocator
	{
	public:

		struct PoolAllocation
		{
			void* m_ptr;
			VkDeviceMemory m_memoryHandle;
			uint32_t m_offset;
			uint32_t m_size;
		};

		PoolAllocator() = default;

		~PoolAllocator();

		void Init(Renderer* renderer, EMemoryType memoryType, uint32_t poolSize, uint32_t minAlignmentBytes, const CLib::Vector<uint32_t>& segments);

		void Alloc(uint32_t sizeBytes, uint32_t alignBytes, PoolAllocation& outAllocation);

		void Free(PoolAllocation& allocation);

	private:

		struct MemoryPool
		{
			size_t m_size;
		};

		static void* PoolAlloc(void* context, uint32_t requestedMinSize, uint32_t& outSize);
		static void PoolFree(void* context, void* ptr);

		Renderer* m_renderer = nullptr;
		VkDevice m_device = VK_NULL_HANDLE;
		uint32_t m_memoryTypeIndex = 0;
		uint32_t m_poolSize = 0;
		uint32_t m_minAlignment = 0;
		EMemoryType m_memoryType = EMemoryType::END_RANGE;
		size_t m_totalAllocatedMemory = 0;
		std::mutex m_mutex;
		std::map<void*, VkDeviceMemory> m_pools;
		CLib::ExternalAllocator m_poolSuballocator{};
	};
}