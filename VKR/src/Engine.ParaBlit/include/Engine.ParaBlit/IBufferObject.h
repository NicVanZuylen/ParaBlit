#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	class IRenderer;
	class IBufferObject;

	struct BufferObjectDesc
	{
		const char* m_name = nullptr;
		BufferOptionFlags m_options{};
		BufferUsageFlags m_usage{};
		u32 m_bufferSize = 0;
	};

	struct BufferViewDesc
	{
		BufferViewDesc() = default;

		BufferViewDesc(u32 offset, u32 size)
			: m_offset(offset)
			, m_size(size)
		{}

		IBufferObject* m_buffer = nullptr;
		u32 m_offset = 0;
		u32 m_size = ~0u;
		u64 m_pad0[2]{};

		bool operator == (const BufferViewDesc& other) const;
	};

	struct BufferCopyRegion
	{
		u64 m_srcOffset;
		u64 m_dstOffset;
		u64 m_size;
	};

	class IBufferObject
	{
	public:

		/*
		Description: Create a new buffer object using a provided renderer and BufferObjectDesc.
		Param:
			IRenderer* renderer: The renderer this buffer will belong to and be usable by.
			const BufferObjectDesc& desc: Structure describing the buffers's properties.
		*/
		PARABLIT_INTERFACE void Create(IRenderer* renderer, const BufferObjectDesc& desc) = 0;

		/*
		Description: Get the size of the buffer (this may not always be the size provided in the descriptor.
		Return Type: u32
		*/
		PARABLIT_INTERFACE u32 GetSize() const = 0;

		/*
		Description: Map the internal contents of the buffer to a user-accessible pointer.
		Return Type: u8*
		Param:
			u32 offset: The offset of the buffer region to map.
			u32 size: The size of the buffer region to map.
		*/
		PARABLIT_INTERFACE u8* Map(u32 offset, u32 size) = 0;

		/*
		Description: Unmap the buffer, do not call this when the buffer isn't currently mapped.
		*/
		PARABLIT_INTERFACE void Unmap() = 0;

		/*
		Description: Map the internal buffer for initial population by the user, works for device local memory.
		Return Type: u8*
		Param:
			u32 size: Size of the writable region of this resource.
		*/
		PARABLIT_INTERFACE u8* BeginPopulate(u32 size = 0) = 0;

		/*
		Description: Get the staging buffer start offset to use for copy regions.
		Return Type: u32
		*/
		PARABLIT_INTERFACE u32 StagingBufferOffset() = 0;

		/*
		Description: Finish populating the buffer with it's initial contents.
		Param:
			u32 writeOffset: The offset in the dest buffer in which the staging buffer will be copied.
		*/
		PARABLIT_INTERFACE void EndPopulate(u32 writeOffset = 0) = 0;

		/*
		Description: Overload of EndPopulate that allows the user to specify regions of the staging buffer to copy.
		Param:
			BufferCopyRegion* regions: Array of structures detailing the regions of the staging buffer to copy.
		*/
		PARABLIT_INTERFACE void EndPopulate(BufferCopyRegion* regions, u32 regionCount) = 0;

		/*
		Description: Map the internal buffer and fill it with the provided data, then unmap it.
		Param:
			u8* data: The data used to fill the buffer.
			u32 size: The size of the data copy.
		*/
		PARABLIT_INTERFACE void Populate(u8* data, u32 size) = 0;

		/*
		Description: Populate the buffer with indirect draw indexed command parameters matching the API format.
		Param:
			PB::u8* location: The location to write the parameters, this should be a mapped pointer of this buffer.
			const DrawIndexedIndirectParams& params: The parameters to format and fill the buffer with.
		*/
		PARABLIT_INTERFACE void PopulateWithDrawIndexedIndirectParams(u8* location, const DrawIndexedIndirectParams& params) = 0;

		/*
		Description: Get the API structure size for DrawIndexedIndirect parameters.
		Return Type: u32
		*/
		PARABLIT_INTERFACE u32 GetDrawIndexedIndirectParamsSize() = 0;

		/*
		Description: Get the size and alignment of this buffer if it were placed on a resource pool.
		Param:
			out u32& size: Output size of the buffer on a resource pool.
			out u32& align: Output alignment of the buffer on a resource pool.
		*/
		PARABLIT_INTERFACE void GetPlacedResourceSizeAndAlign(u32& size, u32& align) = 0;

		/*
		Description: Get a default view of the entire range of this buffer object to use as a uniform buffer.
		Return Type: UniformBufferView
		*/
		PARABLIT_INTERFACE UniformBufferView GetViewAsUniformBuffer() = 0;

		/*
		Description: Get a view of a range of this buffer object to use as a uniform buffer.
		Return Type: UniformBufferView
		Param:
			const BufferViewDesc& viewDesc: Structure describing the view's properties.
		*/
		PARABLIT_INTERFACE UniformBufferView GetViewAsUniformBuffer(BufferViewDesc& viewDesc) = 0;

		/*
		Description: Get a default view of the entire range of this buffer object to use as a shader storage buffer.
		Return Type: ResourceView
		*/
		PARABLIT_INTERFACE ResourceView GetViewAsStorageBuffer() = 0;

		/*
		Description: Get a view of a range of this buffer object to use as a shader storage buffer.
		Return Type: ResourceView
		Param:
			const BufferViewDesc& viewDesc: Structure describing the view's properties.
		*/
		PARABLIT_INTERFACE ResourceView GetViewAsStorageBuffer(BufferViewDesc& viewDesc) = 0;
	};
}