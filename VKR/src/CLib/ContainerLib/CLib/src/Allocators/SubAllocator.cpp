#include "CLib/SubAllocator.h"
#include <iostream>

#if CLIB_WINDOWS
#define CLIB_ALIGNED_MALLOC _aligned_malloc
#define CLIB_ALIGNED_FREE _aligned_free
#elif CLIB_LINUX
#define CLIB_ALIGNED_MALLOC aligned_alloc
#define CLIB_ALIGNED_FREE free
#endif

namespace CLib
{
	//const uint32_t SubAllocator::Page::PageSize = 1024 * 1024;
	const int64_t SubAllocator::BlockMeta::MinSize = 4;

	SubAllocator::SubAllocator()
	{
		CreatePage();
	}

	SubAllocator::~SubAllocator()
	{
		for (uint32_t i = 0; i < m_pages.Count(); ++i)
			CLIB_ALIGNED_FREE(m_pages[i].m_base);
	}

	bool SubAllocator::DumpLeaks()
	{
		Vector<BlockMeta*, 1000> leakedBlocks;
		for (auto& page : m_pages)
		{
			BlockMeta* currentBlock = reinterpret_cast<BlockMeta*>(page.m_base);
			while (currentBlock)
			{
				// Assuming any currently allocated blocks are leaks...
				if (currentBlock->m_allocated)
					leakedBlocks.PushBack(currentBlock);

				if (currentBlock->m_start + currentBlock->m_size >= Page::PageSize)
					break;

				// Jump to next block.
				currentBlock = reinterpret_cast<BlockMeta*>(reinterpret_cast<size_t>(page.m_base) + currentBlock->m_start + currentBlock->m_size);
			}
		}

		if (leakedBlocks.Count() > 0)
		{
			std::cout << "\nSubAllocator: Memory leaks detected! Dumping them here...\n\n";
			for (auto& block : leakedBlocks)
			{
				// Dump leak info
				std::cout << "-----------------------------------------------------\n";
				std::cout << "Block:\n";
				std::cout << "Start: " << block->m_start << std::endl;
				std::cout << "Size: " << block->m_size << std::endl;
				std::cout << "Page: " << block->m_pageIndex << ", Page Address: " << reinterpret_cast<void*>(m_pages[block->m_pageIndex].m_base) << std::endl;
				std::cout << "Address: " << reinterpret_cast<void*>(reinterpret_cast<size_t>(m_pages[block->m_pageIndex].m_base) + block->m_start) << std::endl;
				std::cout << "-----------------------------------------------------\n";
			}
			std::cout << "\nSubAllocator: Finished Dumping memory leaks.\n";
		}
		else
			std::cout << "SubAllocator: No memory leaks detected.\n";

		return leakedBlocks.Count() > 0;
	}

	void* SubAllocator::Alloc(uint32_t size, uint32_t alignment)
	{
		return InternalAlloc(size, alignment);
	}

	void SubAllocator::Free(void* ptr)
	{
		InternalFree(ptr);
	}

	void* SubAllocator::InternalAlloc(uint32_t size, uint32_t alignment)
	{
		// Size plus alignment from padding.
		uint32_t requiredSize = alignment > 0 ? size + (alignment - (size % alignment)) : size;

		if (requiredSize > Page::PageSize - sizeof(BlockMeta)) // If we're exiting here its because the requested allocation is larger than the available memory in an empty page, so its impossible to allocate.
			return nullptr;

		// TODO: Track a table of BlockMeta pointers indexed using sizes. There should be an element for each power of two above 32(???).
		// The table should jump us straight to the first large enough block when found, which should be the smallest tracked block large enough for the allocation.
		// This should prevent large amounts looping though blocks and pages when there are many live allocations to find a suitable free block.

		uint32_t currentPage = 0;
		uint32_t currentPos = 0;
		BlockMeta* currentBlock = reinterpret_cast<BlockMeta*>(m_pages[currentPage].m_base);

		while (currentBlock)
		{
			uint32_t nextPos = currentPos + sizeof(BlockMeta) + currentBlock->m_size;
			if (!currentBlock->m_allocated && currentBlock->m_size >= requiredSize)
			{
				// Ensure alignment requirement is met, by making sure out offset from the previous block meets the alignment.
				if (currentBlock->m_diffFromPrevBlock > 0 && alignment > 0)
				{
					BlockMeta* prevBlock = reinterpret_cast<BlockMeta*>(reinterpret_cast<size_t>(currentBlock) - currentBlock->m_diffFromPrevBlock);
					uint32_t pad = (alignment - (prevBlock->m_start + prevBlock->m_size)) % alignment;
					if (pad > 0)
					{
						// Alignment requirement not met.
						// Increase the size of the previous block, so that the current block meets the alignment requirement.
						prevBlock->m_size += pad;
						currentBlock->m_size -= pad;
						currentBlock->m_start += pad;
						currentBlock->m_diffFromPrevBlock = (currentBlock->m_start - sizeof(BlockMeta)) - (prevBlock->m_start - sizeof(BlockMeta));
						BlockMeta* newCurrent = reinterpret_cast<BlockMeta*>(reinterpret_cast<size_t>(currentBlock) + pad);
						BlockMeta tmpMeta = *currentBlock;
						*newCurrent = tmpMeta;

						currentBlock = newCurrent;
						currentPos += pad;
					}
				}

				uint32_t newEnd = currentPos + sizeof(BlockMeta) + requiredSize;
				int remainingSize = nextPos - newEnd - sizeof(BlockMeta);
				if (remainingSize >= BlockMeta::MinSize)
				{
					// There's enough space to create a new block, trim this one to make room for the next.
					currentBlock->m_size = requiredSize;

					// Create new block
					BlockMeta* newBlock = reinterpret_cast<BlockMeta*>(reinterpret_cast<size_t>(m_pages[currentPage].m_base) + newEnd);
					newBlock->m_size = remainingSize;
					newBlock->m_start = newEnd + sizeof(BlockMeta); // m_start should be the start of the writable memory, and thus won't include the BlockMeta.
					newBlock->m_diffFromPrevBlock = static_cast<uint32_t>(newEnd - currentPos);
					newBlock->m_pageIndex = currentPage;
					newBlock->m_allocated = false;
				}
				
				currentBlock->m_allocated = true;
				return reinterpret_cast<void*>(reinterpret_cast<size_t>(m_pages[currentPage].m_base) + currentBlock->m_start);
			}

			if (nextPos == Page::PageSize)
			{
				currentBlock = nullptr;
				++currentPage;

				// Allocate another page if necessary.
				if (currentPage == m_pages.Count())
					CreatePage();
				currentBlock = reinterpret_cast<BlockMeta*>(m_pages[currentPage].m_base);
				currentPos = 0;
			}
			else
			{
				currentBlock = (BlockMeta*)((size_t)m_pages[currentPage].m_base + nextPos);
				currentPos = nextPos;
			}
		}

		return nullptr;
	}

	void SubAllocator::InternalFree(void* ptr)
	{
		BlockMeta* block = reinterpret_cast<BlockMeta*>((size_t)ptr - sizeof(BlockMeta));
		BlockMeta* prevBlock = reinterpret_cast<BlockMeta*>(reinterpret_cast<size_t>(block) - block->m_diffFromPrevBlock);
		BlockMeta* nextBlock = reinterpret_cast<BlockMeta*>(reinterpret_cast<size_t>(m_pages[block->m_pageIndex].m_base) + block->m_start + block->m_size);

		block->m_allocated = false;

		bool currentBlockIsFinalBlock = block->m_start + block->m_size >= Page::PageSize;
		bool nextBlockIsFinalBlock = !currentBlockIsFinalBlock && nextBlock->m_start + nextBlock->m_size >= Page::PageSize;
		if(nextBlockIsFinalBlock)
		{
			// TODO: Assert here, making sure that nextBlock is not allocated.
			// If we entered this statement this is the final block that was created, that was never allocated.
			block->m_size += sizeof(BlockMeta) + nextBlock->m_size;
		}

		if (currentBlockIsFinalBlock) // There is no next block, so exit here if the current block is the final block on the page.
			return;

		// Merge with the previous block if it is available.
		bool mergeWithPrevBlock = block->m_diffFromPrevBlock > 0 && !prevBlock->m_allocated;
		if (mergeWithPrevBlock)
		{
			prevBlock->m_size += sizeof(BlockMeta) + block->m_size;
		}

		BlockMeta* newCurrentBlock = mergeWithPrevBlock ? prevBlock : block;

		// Merge with the next block if its available. Don't merge if the next block was the final block, as the merge will have already happened.
		if(!nextBlock->m_allocated && !nextBlockIsFinalBlock)
		{
			newCurrentBlock->m_size += sizeof(BlockMeta) + nextBlock->m_size;

			// Set nextBlock to point to nextBlock's next block. Since that is now the actual next block.
			nextBlock = reinterpret_cast<BlockMeta*>(reinterpret_cast<size_t>(m_pages[nextBlock->m_pageIndex].m_base) + nextBlock->m_start + nextBlock->m_size);
		}

		nextBlock->m_diffFromPrevBlock = static_cast<uint32_t>(reinterpret_cast<size_t>(nextBlock) - reinterpret_cast<size_t>(newCurrentBlock));
	}

	void SubAllocator::CreatePage()
	{
		auto& page = m_pages.PushBack();

		page.m_base = reinterpret_cast<u8*>(CLIB_ALIGNED_MALLOC(Page::PageSize, 4));

		// Create first block.
		BlockMeta* block = reinterpret_cast<BlockMeta*>(page.m_base);
		block->m_size = static_cast<uint32_t>(Page::PageSize - sizeof(BlockMeta));
		block->m_diffFromPrevBlock = 0;
		block->m_start = sizeof(BlockMeta);
		block->m_pageIndex = m_pages.Count() - 1;
		block->m_allocated = false;
	}
}
