#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	class IRenderer;

	struct BufferObjectDesc
	{
		BufferOptions m_options{};
		BufferUsage m_usage{};
		u32 m_bufferSize = 0;
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
		PARABLIT_INTERFACE u32 GetSize() = 0;

		/*
		Description: Map the internal contents of the buffer to a user-accessible pointer.
		Return Type: u8*
		Param:
			u32 size: The size of the buffer region to map.
			u32 offset: The offset of the buffer region to map.
		*/
		PARABLIT_INTERFACE u8* Map(u32 size, u32 offset) = 0;

		/*
		Description: Unmap the buffer, do not call this when the buffer isn't currently mapped.
		*/
		PARABLIT_INTERFACE void Unmap() = 0;

		/*
		Description: Map the internal buffer for initial population by the user, works for device local memory.
		Return Type: u8*
		*/
		PARABLIT_INTERFACE u8* BeginPopulate() = 0;

		/*
		Description: Finish populating the buffer with it's initial contents.
		*/
		PARABLIT_INTERFACE void EndPopulate() = 0;

		/*
		Description: Map the internal buffer and fill it with the provided data, then unmap it.
		Param:
			u8* data: The data used to fill the buffer.
			u32 size: The size of the data copy.
		*/
		PARABLIT_INTERFACE void Populate(u8* data, u32 size) = 0;
	};
}