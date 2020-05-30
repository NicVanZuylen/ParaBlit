#pragma once
#include "ParaBlitInterface.h"

namespace PB
{
	enum ETextureState : u16
	{
		PB_TEXTURE_STATE_NONE = 0,
		PB_TEXTURE_STATE_RAW = 1,
		PB_TEXTURE_STATE_RENDERTARGET = 1 << 1,
		PB_TEXTURE_STATE_DEPTHTARGET = 1 << 2,
		PB_TEXTURE_STATE_SAMPLED = 1 << 3,
		PB_TEXTURE_STATE_COPY_SRC = 1 << 4,
		PB_TEXTURE_STATE_COPY_DST = 1 << 5,
		PB_TEXTURE_STATE_PRESENT = 1 << 6,
		PB_TEXTURE_STATE_MAX = 1 << 7
	};

	enum ETextureFormat : u16
	{
		PB_TEXTURE_FORMAT_UNKNOWN = 0,
		PB_TEXTURE_FORMAT_R8_UNORM,
		PB_TEXTURE_FORMAT_R8G8_UNORM,
		PB_TEXTURE_FORMAT_R8G8B8_UNORM,
		PB_TEXTURE_FORMAT_R8G8B8A8_UNORM,
	};

	inline ETextureState operator | (ETextureState a, ETextureState b)
	{
		return static_cast<ETextureState>(static_cast<int>(a) | static_cast<int>(b));
	}

	class ITexture
	{
	public:


	};
}