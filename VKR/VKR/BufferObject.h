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

		u8* BeginPopulate(u32 size = 0) override;

		void EndPopulate(u32 writeOffset = 0) override;

		void Populate(u8* data, u32 size) override;

		void PopulateWithDrawIndexedIndirectParams(u8* location, const DrawIndexedIndirectParams& params) override;

		u32 GetDrawIndexedIndirectParamsSize() override;

		UniformBufferView GetViewAsUniformBuffer() override;

		UniformBufferView GetViewAsUniformBuffer(BufferViewDesc& viewDesc) override;

		ResourceView GetViewAsStorageBuffer() override;

		ResourceView GetViewAsStorageBuffer(BufferViewDesc& viewDesc) override;

		void RegisterView(const BufferViewDesc& desc, EBufferUsage type);

		BufferUsageFlags GetUsage() const;

	private:

		struct BufferViewOwnershipData
		{
			BufferViewDesc m_desc;
			EBufferUsage m_type;
		};

		inline void CreateVkBuffer(const BufferObjectDesc& desc);

		inline void InitializeMemory(const BufferObjectDesc& desc);

		inline void CopyStagingBuffer(const TempBuffer& buffer, const u32& copyOffset);

		Renderer* m_renderer = nullptr;
		VkBuffer m_handle = VK_NULL_HANDLE;
		TempBuffer m_stagingBuffer{};
		DeviceAllocator::PageView m_memoryPage{};
		BufferUsageFlags m_usage = 0;
		u32 m_size;
		CLib::Vector<BufferViewOwnershipData, 1, 4> m_ownedViews;
	};
};

