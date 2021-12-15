#include "BufferObject.h"
#include "Renderer.h"
#include "PBUtil.h"
#include "ParaBlitDebug.h"

namespace PB
{
	void BufferObject::Create(IRenderer* renderer, const BufferObjectDesc& desc)
	{
		m_renderer = reinterpret_cast<Renderer*>(renderer);
		m_usage = desc.m_usage;
		m_size = desc.m_bufferSize;

		CreateVkBuffer(desc);

		if(!(desc.m_options & PB::EBufferOptions::POOL_PLACED))
			InitializeMemory(desc, m_poolAllocation, m_memoryType);
	}

	void BufferObject::Destroy()
	{
		auto device = m_renderer->GetDevice();
		if (m_handle != VK_NULL_HANDLE)
		{
			for (auto& view : m_ownedViews)
			{
				switch (view.m_type)
				{
				case EBufferUsage::UNIFORM:
					m_renderer->GetViewCache()->DestroyUniformBufferView(view.m_desc);
					break;
				case EBufferUsage::STORAGE:
					m_renderer->GetViewCache()->DestroySSBOBufferView(view.m_desc);
					break;
				default:
					PB_NOT_IMPLEMENTED;
					break;
				}
			}

			vkDestroyBuffer(device->GetHandle(), m_handle, nullptr);

			if(m_poolAllocation.m_memoryHandle != VK_NULL_HANDLE)
				device->GetBufferAllocator(m_memoryType).Free(m_poolAllocation);
		}
	}

	VkBuffer BufferObject::GetHandle() const
	{
		return m_handle;
	}

	u32 BufferObject::GetStart() const
	{
		return m_poolAllocation.m_offset;
	}

	u32 BufferObject::GetSize()
	{
		return m_size;
	}

	u8* BufferObject::Map(u32 offset, u32 size)
	{
		PB_ASSERT(m_poolAllocation.m_memoryHandle != VK_NULL_HANDLE && m_poolAllocation.m_ptr != nullptr);

		void* data = nullptr;
		if (m_memoryType == EMemoryType::HOST_VISIBLE)
		{
			data = reinterpret_cast<char*>(m_poolAllocation.m_ptr) + m_poolAllocation.m_offset + offset;
			m_mapOffset = offset;
		}
		return reinterpret_cast<u8*>(data);
	}

	void BufferObject::Unmap()
	{
		PB_ASSERT(m_poolAllocation.m_memoryHandle != VK_NULL_HANDLE && m_poolAllocation.m_ptr != nullptr);

		if (m_memoryType == EMemoryType::HOST_VISIBLE)
		{
			VkMappedMemoryRange memoryRange{};
			memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			memoryRange.pNext = nullptr;
			memoryRange.memory = m_poolAllocation.m_memoryHandle;
			memoryRange.offset = (m_poolAllocation.m_offset + m_mapOffset);
			memoryRange.size = (m_poolAllocation.m_size - m_mapOffset);

			vkFlushMappedMemoryRanges(m_renderer->GetDevice()->GetHandle(), 1, &memoryRange);
		}
	}

	u8* BufferObject::BeginPopulate(u32 size)
	{
		PB_ASSERT(!m_stagingBuffer.m_mappedPtr);
		PB_ASSERT_MSG(m_memoryType != EMemoryType::HOST_VISIBLE, "It is not recommended to use BeginPopulate() for HOST_VISIBLE buffers, as this function creates and maps a staging buffer for copy.");
		PB_ASSERT_MSG(m_usage & EBufferUsage::COPY_DST, "Buffer Object must usable as a copy destination in order to use BeginPopulate().");
		m_stagingBuffer = m_renderer->GetDevice()->GetTempBufferAllocator().NewTempBuffer(size == 0 ? m_size : size, m_renderer->GetCurrentSwapchainImageIndex());
		return m_stagingBuffer.Start();
	}

	u32 BufferObject::StagingBufferOffset()
	{
		return m_stagingBuffer.m_offset;
	}

	void BufferObject::EndPopulate(u32 writeOffset)
	{
		PB_ASSERT(m_stagingBuffer.m_mappedPtr);
		CopyStagingBuffer(m_stagingBuffer, writeOffset);
		m_stagingBuffer.m_mappedPtr = nullptr;
	}

	void BufferObject::EndPopulate(BufferCopyRegion* regions, u32 regionCount)
	{
		PB_ASSERT(m_poolAllocation.m_memoryHandle != VK_NULL_HANDLE && m_poolAllocation.m_ptr != nullptr);

		CommandContext internalContext;
		MakeInternalContext(internalContext, m_renderer);
		internalContext.Begin();

		vkCmdCopyBuffer(internalContext.GetCmdBuffer(), m_stagingBuffer.m_buffer, m_handle, regionCount, reinterpret_cast<const VkBufferCopy*>(regions));

		internalContext.End();
		internalContext.Return();
		m_stagingBuffer.m_mappedPtr = nullptr;
	}

	void BufferObject::Populate(u8* data, u32 size)
	{
		PB_ASSERT(data && size);
		PB_ASSERT(!m_stagingBuffer.m_mappedPtr);
		m_stagingBuffer = m_renderer->GetDevice()->GetTempBufferAllocator().NewTempBuffer(m_size, m_renderer->GetCurrentSwapchainImageIndex());
		memcpy(m_stagingBuffer.Start(), data, size);

		CopyStagingBuffer(m_stagingBuffer, 0);
		m_stagingBuffer.m_mappedPtr = nullptr;
	}

	void BufferObject::PopulateWithDrawIndexedIndirectParams(u8* location, const DrawIndexedIndirectParams& params)
	{
		PB_ASSERT_MSG(params.offset + sizeof(VkDrawIndexedIndirectCommand) <= m_poolAllocation.m_size, "Provided offset would extend the parameters past the end of the buffer.");
		PB_ASSERT(location);
		VkDrawIndexedIndirectCommand& data = *reinterpret_cast<VkDrawIndexedIndirectCommand*>(location);
		data.indexCount = params.indexCount;
		data.instanceCount = params.instanceCount;
		data.firstIndex = params.firstIndex;
		data.vertexOffset = params.vertexOffset;
		data.firstInstance = params.firstInstance;
	}

	u32 BufferObject::GetDrawIndexedIndirectParamsSize()
	{
		return sizeof(VkDrawIndexedIndirectCommand);
	}

	void BufferObject::GetPlacedResourceSizeAndAlign(u32& size, u32& align)
	{
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(m_renderer->GetDevice()->GetHandle(), m_handle, &memRequirements);

		size = static_cast<u32>(memRequirements.size);
		align = static_cast<u32>(memRequirements.alignment);
	}

	UniformBufferView BufferObject::GetViewAsUniformBuffer()
	{
		BufferViewDesc viewDesc;
		viewDesc.m_buffer = this;
		viewDesc.m_offset = 0;
		viewDesc.m_size = m_size;
		return m_renderer->GetViewCache()->GetUniformBufferView(viewDesc);
	}

	UniformBufferView BufferObject::GetViewAsUniformBuffer(BufferViewDesc& viewDesc)
	{
		viewDesc.m_buffer = this;
		return m_renderer->GetViewCache()->GetUniformBufferView(viewDesc);
	}

	ResourceView BufferObject::GetViewAsStorageBuffer()
	{
		BufferViewDesc viewDesc;
		viewDesc.m_buffer = this;
		viewDesc.m_offset = 0;
		viewDesc.m_size = m_size;
		return m_renderer->GetViewCache()->GetSSBOBufferView(viewDesc);
	}

	ResourceView BufferObject::GetViewAsStorageBuffer(BufferViewDesc& viewDesc)
	{
		viewDesc.m_buffer = this;
		return m_renderer->GetViewCache()->GetSSBOBufferView(viewDesc);
	}

	void BufferObject::RegisterView(const BufferViewDesc& desc, EBufferUsage type)
	{
		auto& ownedView = m_ownedViews.PushBack();
		ownedView.m_desc = desc;
		ownedView.m_type = type;
	}

	BufferUsageFlags BufferObject::GetUsage() const
	{
		return m_usage;
	}

	inline void BufferObject::CreateVkBuffer(const BufferObjectDesc& desc)
	{
		auto device = m_renderer->GetDevice();

		VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
		bufferInfo.flags = 0;
		bufferInfo.queueFamilyIndexCount = 1;
		u32 graphicsQueue = device->GetGraphicsQueueFamilyIndex();
		bufferInfo.pQueueFamilyIndices = &graphicsQueue;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.size = desc.m_bufferSize;

		bufferInfo.usage = ConvertPBBufferUsageToVkBufferUsage(desc.m_usage);

		// Add transfer dst if this is a device local buffer which is zero-initialized.
		if ((desc.m_options & EBufferOptions::ZERO_INITIALIZE) && !(desc.m_options & EBufferOptions::CPU_ACCESSIBLE))
		{
			bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			m_usage |= EBufferUsage::COPY_DST;
		}

		PB_ASSERT(m_handle == VK_NULL_HANDLE);
		PB_ERROR_CHECK(vkCreateBuffer(device->GetHandle(), &bufferInfo, nullptr, &m_handle));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_handle != VK_NULL_HANDLE);

		if (!(desc.m_options & PB::EBufferOptions::POOL_PLACED)) // Skip memory allocation & binding for placed buffers, as they will later be placed on a memory heap.
		{
			VkMemoryRequirements bufferMemRequirements;
			vkGetBufferMemoryRequirements(device->GetHandle(), m_handle, &bufferMemRequirements);
			m_memoryType = EMemoryType::DEVICE_LOCAL;
			if (desc.m_options & EBufferOptions::CPU_ACCESSIBLE)
				m_memoryType = EMemoryType::HOST_VISIBLE;

			device->GetBufferAllocator(m_memoryType).Alloc(uint32_t(bufferMemRequirements.size), uint32_t(bufferMemRequirements.alignment), m_poolAllocation);

			PB_ERROR_CHECK(vkBindBufferMemory(device->GetHandle(), m_handle, m_poolAllocation.m_memoryHandle, m_poolAllocation.m_offset));
			PB_BREAK_ON_ERROR;
		}
	}

	inline void BufferObject::InitializeMemory(const BufferObjectDesc& desc, const PoolAllocator::PoolAllocation& allocation, EMemoryType memoryType)
	{
		if (desc.m_options & EBufferOptions::CPU_ACCESSIBLE)
		{
			if (desc.m_options & EBufferOptions::ZERO_INITIALIZE)
			{
				u8* mapped = Map(0, allocation.m_size); // We don't need a staging buffer since this buffer's memory is host visible.
				memset(mapped, 0, allocation.m_size);
				Unmap();
			}
		}
		else
		{
			if (desc.m_options & EBufferOptions::ZERO_INITIALIZE)
			{
				auto device = m_renderer->GetDevice();

				// Since this buffer's memory is not host visible, we'll need to create a temporary host visible staging buffer to zero-initialize, and copy over this one,
				auto stagingBuffer = device->GetTempBufferAllocator().NewTempBuffer(allocation.m_size, m_renderer->GetCurrentSwapchainImageIndex());
				memset(stagingBuffer.Start(), 0, allocation.m_size);

				CopyStagingBuffer(stagingBuffer, 0);
			}
		}
	}

	inline void BufferObject::CopyStagingBuffer(const TempBuffer& buffer, const u32& writeOffset)
	{
		CommandContext internalContext;
		MakeInternalContext(internalContext, m_renderer);
		internalContext.Begin();

		// Staging buffers don't use the IBufferObject API, so we'll have to issue the copy command here.
		u32 copySize = m_size < buffer.m_size ? m_size : buffer.m_size;
		VkBufferCopy copyRegion{ buffer.m_offset, writeOffset, copySize };
		vkCmdCopyBuffer(internalContext.GetCmdBuffer(), buffer.m_buffer, m_handle, 1, &copyRegion);

		internalContext.End();
		internalContext.Return();
	}
};