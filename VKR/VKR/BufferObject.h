#pragma once
#include "IBufferObject.h"
#include "ParaBlitApi.h"
#include "DeviceAllocator.h"
#include "StagingBufferAllocator.h"

namespace PB
{
	class IRenderer;
	class Renderer;

	class BufferObject : public IBufferObject
	{
	public:

		void Create(IRenderer* renderer, const BufferObjectDesc& desc) override;

		void Destroy();

		VkBuffer GetHandle() const;

		u32 GetStart() const;

		u32 GetSize() override;

		u8* Map(u32 offset, u32 size) override;

		void Unmap() override;

		u8* BeginPopulate() override;

		void EndPopulate() override;

		void Populate(u8* data, u32 size) override;

		BufferView GetView() override;

		BufferView GetView(BufferViewDesc& viewDesc) override;

		void RegisterView(const BufferViewDesc& desc);

		BufferUsageFlags GetUsage() const;

	private:

		inline void CreateVkBuffer(const BufferObjectDesc& desc);

		inline void InitializeMemory(const BufferObjectDesc& desc);

		inline void CopyStagingBuffer(const TempBuffer& buffer);

		Renderer* m_renderer = nullptr;
		VkBuffer m_handle = VK_NULL_HANDLE;
		TempBuffer m_stagingBuffer{};
		DeviceAllocator::PageView m_memoryPage{};
		BufferUsageFlags m_usage = 0;
		u32 m_size;
		CLib::Vector<BufferViewDesc, 1, 4> m_viewDescs;
	};
};

