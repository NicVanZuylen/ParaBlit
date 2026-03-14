#pragma once
#include "ParaBlitDefs.h"
#include "ParaBlitDebug.h"
#include "CLib/Allocator.h"

namespace PB
{
	class Device;

	struct TempBuffer
	{
		u8* Start() const { return m_mappedPtr + m_offset; }
		u32 RealSize() const { return m_size - m_alignment; };

		VkBuffer m_buffer;
		u8* m_mappedPtr;
		u32 m_offset;
		u32 m_size;
		u32 m_alignment;
	};

	class TempBufferAllocator
	{
	public:

		void ResetFrame(u32 frameIndex);

		void Create(Device* device);

		void Destroy();

		// Allocate temporary buffer. Lifetime is for as long as the buffer's frame it was allocated for is in flight.
		TempBuffer NewTempBuffer(u32 size, u32 frameIndex, EMemoryType memoryType = EMemoryType::HOST_VISIBLE, u32 alignment = 4);

	private:

		static constexpr const u32 MemoryTypeCount = static_cast<u32>(EMemoryType::END_RANGE);
		static constexpr const u32 MinPageSize = 64 * 1024 * 1024; // 64 MB, pages should be huge in order to avoid a large contribution to the device memory allocation limit.
		static constexpr const u32 PageAlignment = 64;

		struct InternalBuffer
		{
			VkBuffer m_buffer = VK_NULL_HANDLE;
			VkDeviceMemory m_memory = VK_NULL_HANDLE;
			u32 m_size = 0;
			u32 m_allocated = 0;
			u64 m_lastUsedFrame = 0;
		};

		struct PageListNode
		{
			void Unlink();
			bool CanFit(u32 size);
			void BuildView(TempBuffer& view, u32 size, u32 alignment);

			VkBuffer m_buffer;
			VkDeviceMemory m_memory;
			u8* m_mappedMemory;
			u32 m_size;
			u32 m_bytesAllocated;

			// Links
			PageListNode* m_prev;
			PageListNode* m_next;
		};

		struct PageList
		{
			inline void Invalidate() { m_start = nullptr; m_end = nullptr; };
			inline bool IsEmpty() { PB_ASSERT(m_start != nullptr || m_end == nullptr); return m_start == nullptr; };
			void AppendNode(PageListNode* node);
			void AppendList(PageList& list);
			void SwapNodes(PageListNode* first, PageListNode* second);
			void UnlinkNode(PageListNode* node);
			PageListNode* UnlinkEnd();

			PageListNode* m_start;
			PageListNode* m_end;
		};

		inline PageListNode* AllocatePageBuffer(EMemoryType memoryType, u32 desiredSize);

		Device* m_device = nullptr;
		std::mutex m_mutex;
		CLib::Allocator m_pageAllocator{ sizeof(PageListNode) * 8 };

		PageList m_freeLists[MemoryTypeCount]{}; // Pool of pages available for any frame to use.
		PageList m_pendingFreeLists[PB_FRAME_IN_FLIGHT_COUNT + 1][MemoryTypeCount]{};
		PageList m_frameLists[PB_FRAME_IN_FLIGHT_COUNT + 1][MemoryTypeCount]{}; // Pages belonging to a frame in flight.
	};
}