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
			for (auto& viewDesc : m_viewDescs)
				m_renderer->GetViewCache()->DestroyBufferView(viewDesc);

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
		if (m_memoryPage.m_memoryType == PB_MEMORY_TYPE_HOST_VISIBLE)
		{
			vkMapMemory(m_renderer->GetDevice()->GetHandle(), m_memoryPage.m_memory, static_cast<VkDeviceSize>(m_memoryPage.AlignedOffset()) + offset, size, 0, &data);
		}
		return reinterpret_cast<u8*>(data);
	}

	void BufferObject::Unmap()
	{
		if(m_memoryPage.m_memoryType == PB_MEMORY_TYPE_HOST_VISIBLE)
			vkUnmapMemory(m_renderer->GetDevice()->GetHandle(), m_memoryPage.m_memory);
	}

	u8* BufferObject::BeginPopulate()
	{
		PB_ASSERT(!m_stagingBuffer.m_parentBuffer && !m_stagingBuffer.m_parentMemory);
		PB_ASSERT_MSG(m_usage & PB_BUFFER_USAGE_COPY_DST, "Buffer Object must usable as a copy destination in order to use BeginPopulate().");
		m_stagingBuffer = m_renderer->GetDevice()->GetTempBufferAllocator().NewTempBuffer(m_size, m_renderer->GetCurrentFrame());
		return m_stagingBuffer.Map(m_renderer->GetDevice()->GetHandle());
	}

	void BufferObject::EndPopulate()
	{
		PB_ASSERT(m_stagingBuffer.m_parentBuffer && m_stagingBuffer.m_parentMemory);
		m_stagingBuffer.Unmap(m_renderer->GetDevice()->GetHandle());
		CopyStagingBuffer(m_stagingBuffer);
		m_stagingBuffer.m_parentBuffer = VK_NULL_HANDLE;
		m_stagingBuffer.m_parentMemory = VK_NULL_HANDLE;
	}

	void BufferObject::Populate(u8* data, u32 size)
	{
		PB_ASSERT(data && size);
		PB_ASSERT(!m_stagingBuffer.m_parentBuffer && !m_stagingBuffer.m_parentMemory);
		m_stagingBuffer = m_renderer->GetDevice()->GetTempBufferAllocator().NewTempBuffer(m_size, m_renderer->GetCurrentFrame());
		auto vkDevice = m_renderer->GetDevice()->GetHandle();
		u8* stagingData = m_stagingBuffer.Map(vkDevice);
		memcpy(stagingData, data, size);
		m_stagingBuffer.Unmap(vkDevice);

		CopyStagingBuffer(m_stagingBuffer);
		m_stagingBuffer.m_parentBuffer = VK_NULL_HANDLE;
		m_stagingBuffer.m_parentMemory = VK_NULL_HANDLE;
	}

	BufferView BufferObject::GetView()
	{
		BufferViewDesc viewDesc;
		viewDesc.m_buffer = this;
		viewDesc.m_offset = 0;
		viewDesc.m_size = m_size;
		return m_renderer->GetViewCache()->GetBufferView(viewDesc);
	}

	BufferView BufferObject::GetView(BufferViewDesc& viewDesc)
	{
		viewDesc.m_buffer = this;
		return m_renderer->GetViewCache()->GetBufferView(viewDesc);
	}

	void BufferObject::RegisterView(const BufferViewDesc& desc)
	{
		m_viewDescs.PushBack() = desc;
	}

	BufferUsage BufferObject::GetUsage() const
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
		if ((desc.m_options & PB_BUFFER_OPTION_ZERO_INITIALIZE) && !(desc.m_options & PB_BUFFER_OPTION_CPU_ACCESSIBLE))
		{
			bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			m_usage |= PB_BUFFER_USAGE_COPY_DST;
		}

		PB_ASSERT(m_handle == VK_NULL_HANDLE);
		PB_ERROR_CHECK(vkCreateBuffer(device->GetHandle(), &bufferInfo, nullptr, &m_handle));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_handle != VK_NULL_HANDLE);

		VkMemoryRequirements bufferMemRequirements;
		vkGetBufferMemoryRequirements(device->GetHandle(), m_handle, &bufferMemRequirements);
		EMemoryType memType = PB_MEMORY_TYPE_DEVICE_LOCAL;
		if (desc.m_options & PB_BUFFER_OPTION_CPU_ACCESSIBLE)
			memType = PB_MEMORY_TYPE_HOST_VISIBLE;

		m_memoryPage = device->GetDeviceAllocator().Alloc(bufferMemRequirements, memType, desc.m_bufferSize);

		PB_ERROR_CHECK(vkBindBufferMemory(device->GetHandle(), m_handle, m_memoryPage.m_memory, m_memoryPage.AlignedOffset()));
		PB_BREAK_ON_ERROR;
	}

	inline void BufferObject::InitializeMemory(const BufferObjectDesc& desc)
	{
		if (desc.m_options & PB_BUFFER_OPTION_CPU_ACCESSIBLE)
		{
			if (desc.m_options & PB_BUFFER_OPTION_ZERO_INITIALIZE)
			{
				u8* mapped = Map(0, desc.m_bufferSize); // We don't need a staging buffer since this buffer's memory is host visible.
				memset(mapped, 0, desc.m_bufferSize);
				Unmap();
			}
		}
		else
		{
			if (desc.m_options & PB_BUFFER_OPTION_ZERO_INITIALIZE)
			{
				auto device = m_renderer->GetDevice();

				// Since this buffer's memory is not host visible, we'll need to create a temporary host visible staging buffer to zero-initialize, and copy over this one,
				auto stagingBuffer = device->GetTempBufferAllocator().NewTempBuffer(desc.m_bufferSize, m_renderer->GetCurrentFrame());

				u8* mapped = stagingBuffer.Map(device->GetHandle());
				memset(mapped, 0, desc.m_bufferSize);
				stagingBuffer.Unmap(device->GetHandle());

				CopyStagingBuffer(stagingBuffer);
			}
		}
	}

	inline void BufferObject::CopyStagingBuffer(const TempBuffer& buffer)
	{
		CommandContext internalContext;
		MakeInternalContext(internalContext, m_renderer);
		internalContext.Begin();

		PB_ASSERT(buffer.m_size <= m_size);

		// Staging buffers don't use the IBufferObject API, so we'll have to issue the copy command here.
		VkBufferCopy copyRegion{ buffer.m_offset, 0, buffer.m_size };
		vkCmdCopyBuffer(internalContext.GetCmdBuffer(), buffer.m_parentBuffer, m_handle, 1, &copyRegion);

		internalContext.End();
		internalContext.Return();
	}
};