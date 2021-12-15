#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	class IBufferObject;
	class ITexture;

	struct ResourcePoolDesc
	{
		u32 m_size = 0;
		PB::EMemoryType m_memoryType = PB::EMemoryType::END_RANGE;
	};

	class IResourcePool
	{
	public:

		/*
		Description: Assign memory at the specified offset to the provided buffer object.
		Param:
			IBufferObject* buffer: The buffer object to assign the memory at 'offset' to.
			u32 offset: The location in the ResourcePool's memory to be assigned to 'buffer'.
		*/
		PARABLIT_INTERFACE void PlaceBuffer(IBufferObject* buffer, u32 offset) = 0;

		/*
		Description: Assign memory at the specified offset to the provided texture object.
		Param:
			ITexture* texture: The texture object to assign the memory at 'offset' to.
			u32 offset: The location in the ResourcePool's memory to be assigned to 'texture'.
		*/
		PARABLIT_INTERFACE void PlaceTexture(ITexture* texture, u32 offset) = 0;
	};
}