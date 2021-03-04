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
		InitializeMemory(desc);
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
			PB_ASSERT(m_memoryPage.m_memory != VK_NULL_HANDLE);
			device->GetDeviceAllocator().Free(m_memoryPage);
		}
	}

	VkBuffer BufferObject::GetHandle() const
	{
		return m_handle;
	}

	u32 BufferObject::GetStart() const
	{
		return m_memoryPage.AlignedOffset();
	}

	u32 BufferObject::GetSize()
	{
		return m_size;
	}

	u8* BufferObject::Map(u32 offset, u32 size)
	{
		void* data = nullptr;
		if (m_memoryPage.m_memoryType == EMemoryType::HOST_VISIBLE)
		{
			vkMapMemory(m_renderer->GetDevice()->GetHandle(), m_memoryPage.m_memory, static_cast<VkDeviceSize>(m_memoryPage.AlignedOffset()) + offset, size, 0, &data);
		}
		return reinterpret_cast<u8*>(data);
	}

	void BufferObject::Unmap()
	{
		if(m_memoryPage.m_memoryType == EMemoryType::HOST_VISIBLE)
			vkUnmapMemory(m_renderer->GetDevice()->GetHandle(), m_memoryPage.m_memory);
	}

	u8* BufferObject::BeginPopulate(u32 size)
	{
		PB_ASSERT(!m_stagingBuffer.m_mappedPtr);
		PB_ASSERT_MSG(m_memoryPage.m_memoryType != EMemoryType::HOST_VISIBLE, "It is not recommended to use BeginPopulate() for HOST_VISIBLE buffers, as this function creates and maps a staging buffer for copy.");
		PB_ASSERT_MSG(m_usage & EBufferUsage::COPY_DST, "Buffer Object must usable as a copy destination in order to use BeginPopulate().");
		m_stagingBuffer = m_renderer->GetDevice()->GetTempBufferAllocator().NewTempBuffer(size == 0 ? m_size : size, m_renderer->GetCurrentSwapchainImageIndex());
		return m_stagingBuffer.Start();
	}

	void BufferObject::EndPopulate(u32 writeOffset)
	{
		PB_ASSERT(m_stagingBuffer.m_mappedPtr);
		CopyStagingBuffer(m_stagingBuffer, writeOffset);
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
		PB_ASSERT_MSG(params.offset + sizeof(VkDrawIndexedIndirectCommand) <= m_memoryPage.m_size, "Provided offset would extend the parameters past the end of the buffer.");
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

		VkMemoryRequirements bufferMemRequirements;
		vkGetBufferMemoryRequirements(device->GetHandle(), m_handle, &bufferMemRequirements);
		EMemoryType memType = EMemoryType::DEVICE_LOCAL;
		if (desc.m_options & EBufferOptions::CPU_ACCESSIBLE)
			memType = EMemoryType::HOST_VISIBLE;

		m_memoryPage = device->GetDeviceAllocator().Alloc(bufferMemRequirements, memType, desc.m_bufferSize);

		PB_ERROR_CHECK(vkBindBufferMemory(device->GetHandle(), m_handle, m_memoryPage.m_memory, m_memoryPage.AlignedOffset()));
		PB_BREAK_ON_ERROR;
	}

	inline void BufferObject::InitializeMemory(const BufferObjectDesc& desc)
	{
		if (desc.m_options & EBufferOptions::CPU_ACCESSIBLE)
		{
			if (desc.m_options & EBufferOptions::ZERO_INITIALIZE)
			{
				u8* mapped = Map(0, desc.m_bufferSize); // We don't need a staging buffer since this buffer's memory is host visible.
				memset(mapped, 0, desc.m_bufferSize);
				Unmap();
			}
		}
		else
		{
			if (desc.m_options & EBufferOptions::ZERO_INITIALIZE)
			{
				auto device = m_renderer->GetDevice();

				// Since this buffer's memory is not host visible, we'll need to create a temporary host visible staging buffer to zero-initialize, and copy over this one,
				auto stagingBuffer = device->GetTempBufferAllocator().NewTempBuffer(desc.m_bufferSize, m_renderer->GetCurrentSwapchainImageIndex());
				memset(stagingBuffer.Start(), 0, desc.m_bufferSize);

				CopyStagingBuffer(stagingBuffer, 0);
			}
		}
	}

	inline void BufferObject::CopyStagingBuffer(const TempBuffer& buffer, const u32& writeOffset)
	{
		CommandContext internalContext;
		MakeInternalContext(internalContext, m_renderer);
		internalContext.Begin();

		PB_ASSERT(buffer.m_size <= m_size - writeOffset);

		// Staging buffers don't use the IBufferObject API, so we'll have to issue the copy command here.
		VkBufferCopy copyRegion{ buffer.m_offset, writeOffset, buffer.m_size };
		vkCmdCopyBuffer(internalContext.GetCmdBuffer(), buffer.m_buffer, m_handle, 1, &copyRegion);

		internalContext.End();
		internalContext.Return();
	}
};