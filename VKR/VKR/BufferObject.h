#pragma once
#include "IBufferObject.h"
#include "ParaBlitApi.h"
#include "DeviceAllocator.h"
#include "StagingBufferAllocator.h"

#include "vulkan/vulkan.h"

namespace PB
{
	class IRenderer;
	class Renderer;

	class BufferObject : public IBufferObject
	{
	public:

		void Create(IRenderer* renderer, const BufferObjectDesc& desc) override;

		void Destroy();

		VkBuffer GetHandle();

		u32 GetStart();

		u32 GetSize() override;

		u8* Map(u32 offset, u32 size) override;

		void Unmap() override;

		u8* BeginPopulate() override;

		void EndPopulate() override;

		void Populate(u8* data, u32 size) override;

	private:

		inline void CreateVkBuffer(const BufferObjectDesc& desc);

		inline void InitializeMemory(const BufferObjectDesc& desc);

		inline void CopyStagingBuffer(const StagingBuffer& buffer);

		Renderer* m_renderer = nullptr;
		VkBuffer m_handle = VK_NULL_HANDLE;
		StagingBuffer m_stagingBuffer{};
		DeviceAllocator::PageView m_memoryPage{};
	};
};

